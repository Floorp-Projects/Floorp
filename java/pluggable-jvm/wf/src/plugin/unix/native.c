/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: native.c,v 1.2 2001/07/12 19:58:24 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

/* Waterfall headers */
#include "jvmp.h"
#include "jvmp_threading.h"
/* some native methods implemented here */
#include "sun_jvmp_generic_motif_Plugin.h"
#include "sun_jvmp_generic_motif_PthreadSynchroObject.h"
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stropts.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <Xm/VendorS.h>
#include <dlfcn.h>
#include <sys/param.h>  
/* AWT glueing code */
#include "awt_Plugin.h"

/* code resolved from AWT DLL */
static void (* LockIt)(JNIEnv *) = NULL;
static void (* UnLockIt)(JNIEnv *) = NULL;
static void (* NoFlushUnlockIt)(JNIEnv *) = NULL;
/* AWT X server connection */
static Display *display;
/* AWT DLL handle */
static void *awtDLL = NULL;

#define AWTDLL "libawt.so"

/* those functions exported from AWT DLL ... */
typedef void (*getAwtLockFunctions_t)(void (**AwtLock)(JNIEnv *), 
				      void (**AwtUnlock)(JNIEnv *),
				      void (**AwtNoFlushUnlock)(JNIEnv *),
				      void *reserved);
typedef void (*getAwtData_t) (int      *awt_depth, 
			      Colormap *awt_cmap, 
			      Visual*  *awt_visual,
			      int      *awt_num_colors, 
			      void*    pReserved);
typedef Display* (*getAwtDisplay_t)(void);

/* ...and written to this table */
struct awt_callbacks_t {
  getAwtLockFunctions_t getAwtLockFunctions;
  getAwtData_t          getAwtData;
  getAwtDisplay_t       getAwtDisplay;
} awt = 
{ NULL,
  NULL,
  NULL };

static WidgetClass getVendorShellWidgetClass() {
  static  WidgetClass *v = NULL;
  if (v != NULL) return *v;
  v = (WidgetClass *) dlsym(RTLD_DEFAULT , "vendorShellWidgetClass");
  if (v != NULL) return *v;
  /* I dunno why but it doesn't work on Linux 
     with statically linked Motif - so search it in libawt explicity */
  v = (WidgetClass *) dlsym(awtDLL, "vendorShellWidgetClass");
  if (v != NULL) return *v;
  fprintf(stderr, 
	  "Cannot resolve vendorShellWidgetClass now: %s\nAborting...\n",
	  dlerror());
  /* XXX: maybe give up more nicely */
  exit(1);
  /* Not reached anyway */
  return *v;
}

static int initAWTGlue() 
{
  static int inited = 0;
#define AWT_RESOLVE(method) awt.##method = \
    (method##_t) dlsym(awtDLL, #method); \
  if (awt.##method == NULL)  { \
    fprintf(stderr, "dlsym: %s", dlerror()); \
    return 0; \
  }
  char awtPath[MAXPATHLEN];

  if (inited) return 1;
  /* JAVA_HOME/JAVAHOME set by Waterfall before, see java_plugin.c */
#ifdef _JVMP_SUNJVM
  sprintf(awtPath, "%s/lib/" ARCH "/" AWTDLL,
	  getenv("JAVA_HOME"));
#endif
#ifdef _JVMP_IBMJVM
  sprintf(awtPath, "%s/bin/" AWTDLL,
	  getenv("JAVAHOME"));
#endif
  /* fprintf(stderr,"loading %s\n", awtPath); */
  awtDLL = dlopen(awtPath, RTLD_NOW);
  if (awtDLL == NULL) 
    { 
      fprintf(stderr,"cannot load AWT: %s\n", dlerror()); 
      return 0;
    } 
  AWT_RESOLVE(getAwtLockFunctions);
  awt.getAwtLockFunctions(&LockIt, &UnLockIt, &NoFlushUnlockIt, NULL);
  AWT_RESOLVE(getAwtData);
  AWT_RESOLVE(getAwtDisplay);
  inited = 1;
  return 1;
}

static void
checkPos(Widget w, XtPointer data, XEvent *event)
{
  /* for reparent hack */
  w->core.x = event->xcrossing.x_root - event->xcrossing.x;
  w->core.y = event->xcrossing.y_root - event->xcrossing.y;
}


JNIEXPORT jlong JNICALL
Java_sun_jvmp_generic_motif_Plugin_getWidget(JNIEnv *env, jclass clz,
					     jint winid, jint width, jint height, 
					     jint x, jint y)
{
    Arg args[40];
    int argc;
    Widget w;
    Window child, parent;
    Visual *visual;
    Colormap cmap;
    int depth;
    int ncolors;
    Display **awt_display_ptr;

    /*
     * Create a top-level shell.  Note that we need to use the
     * AWT's own awt_display to initialize the widget.  If we
     * try to create a second X11 display connection the Java
     * runtimes get very confused.
     */
    (*LockIt)(env);
    argc = 0;
    XtSetArg(args[argc], XmNsaveUnder, False); argc++;
    XtSetArg(args[argc], XmNallowShellResize, False); argc++;

    /* the awt initialization should be done by now (awt_GraphicsEnv.c) */

    awt.getAwtData(&depth,&cmap, &visual, &ncolors, NULL);

    awt_display_ptr = (Display **) dlsym(awtDLL, "awt_display");
    if (awt_display_ptr == NULL) 
	display = awt.getAwtDisplay();
    else 
	display = *awt_display_ptr;

    XtSetArg(args[argc], XmNvisual, visual); argc++;
    XtSetArg(args[argc], XmNdepth, depth); argc++;
    XtSetArg(args[argc], XmNcolormap, cmap); argc++;

    XtSetArg(args[argc], XmNwidth, width); argc++;
    XtSetArg(args[argc], XmNheight, height); argc++;
    XtSetArg(args[argc], XmNx, 0); argc++;
    XtSetArg(args[argc], XmNy, 0); argc++;

    XtSetArg(args[argc], XmNmappedWhenManaged, False); argc++;

    w = XtAppCreateShell("AWTapp","XApplication",
			 getVendorShellWidgetClass(),
			 display,
			 args,
			 argc);
    XtRealizeWidget(w);
    XtAddEventHandler(w, EnterWindowMask, FALSE,(XtEventHandler) checkPos, 0);
    /*
     * Now reparent our new Widget into our Navigator window
     */
    parent = (Window) winid;
    child = XtWindow(w);
    XReparentWindow(display, child, parent, 0, 0);
    XFlush(display);
    XSync(display, False); 
    XtVaSetValues(w, XmNx, 0, XmNy, 0, NULL);
    XFlush(display);
    XSync(display, False);
    (*UnLockIt)(env);	
    return PTR_TO_JLONG(w);
}

/** the only reason of this #if is request of pjava team - they don't have
 *  working JNI_OnLoad. If it will be fixed - remove it at all, as it looks 
 *  nasty.
 **/
#if (defined WF_PJAVA)
#if (defined __GNUC__)
void init() __attribute__ ((constructor));
void init()
{
  fprintf(stderr, "pJava-ready WF native code\n");
  initAWTGlue();
}
#else
#error "Implement constructor-like init() function for your compiler"
#endif
#else
JNIEXPORT jint JNICALL 
JNI_OnLoad(JavaVM *vm, void *reserved)
{ 
  if (!initAWTGlue()) return 0;
  return JNI_VERSION_1_2; 
} 
#endif


static jlong getTimeMillis()
{
    struct timeval t;
    gettimeofday(&t, 0);
    return ((jlong)t.tv_sec) * 1000 + (jlong)(t.tv_usec/1000);
}

JNIEXPORT jint JNICALL 
Java_sun_jvmp_generic_motif_PthreadSynchroObject_checkHandle (JNIEnv *env, 
							      jobject jobj, 
							      jlong handle)
{
  JVMP_Monitor* monitor = (JVMP_Monitor*)JLONG_TO_PTR(handle);

  if (!monitor || monitor->magic != JVMP_MONITOR_MAGIC) return -1;
  return 0;
}

JNIEXPORT jint JNICALL 
Java_sun_jvmp_generic_motif_PthreadSynchroObject_doDestroy(JNIEnv *env, 
							   jobject jobj, 
							   jlong   handle)
{
  JVMP_Monitor* monitor = (JVMP_Monitor*)JLONG_TO_PTR(handle);
  /* XXX: should this code destroy anything at all? */
  pthread_cond_destroy((pthread_cond_t*)monitor->monitor);
  pthread_mutex_destroy((pthread_mutex_t*)monitor->mutex);
  return 0;
}

JNIEXPORT jint JNICALL 
Java_sun_jvmp_generic_motif_PthreadSynchroObject_doLock(JNIEnv *env, 
							jobject jobj, 
							jlong handle)
{
  JVMP_Monitor* monitor = (JVMP_Monitor*)JLONG_TO_PTR(handle);
  int r;

  if (monitor->mutex_state != JVMP_STATE_INITED) 
      return JVMP_ERROR_INCORRECT_MUTEX_STATE;
  r = pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);
  if (r != 0) return r;
  monitor->mutex_state = JVMP_STATE_LOCKED;
  return 0;
}

JNIEXPORT jint JNICALL 
Java_sun_jvmp_generic_motif_PthreadSynchroObject_doUnlock(JNIEnv *env, 
							  jobject jobj, 
							  jlong handle)
{
  JVMP_Monitor* monitor = (JVMP_Monitor*)JLONG_TO_PTR(handle);
  int r;

  if (monitor->mutex_state != JVMP_STATE_LOCKED) 
      return JVMP_ERROR_MUTEX_NOT_LOCKED;
  r = pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);
  if (r != 0) return r;
  monitor->mutex_state = JVMP_STATE_INITED;
  return 0;
}

JNIEXPORT jint JNICALL 
Java_sun_jvmp_generic_motif_PthreadSynchroObject_doNotify(JNIEnv *env, 
							  jobject jobj, 
							  jlong handle)
{
  JVMP_Monitor* monitor = (JVMP_Monitor*)JLONG_TO_PTR(handle);

  if (monitor->mutex_state != JVMP_STATE_LOCKED)
      return JVMP_ERROR_MUTEX_NOT_LOCKED;
  return pthread_cond_signal((pthread_cond_t*)monitor->monitor);
}

JNIEXPORT jint JNICALL 
Java_sun_jvmp_generic_motif_PthreadSynchroObject_doNotifyAll(JNIEnv *env, 
							     jobject jobj, 
							     jlong handle)
{
  JVMP_Monitor* monitor = (JVMP_Monitor*)JLONG_TO_PTR(handle);
  if (monitor->mutex_state != JVMP_STATE_LOCKED)
      return JVMP_ERROR_MUTEX_NOT_LOCKED;
  return pthread_cond_broadcast((pthread_cond_t*)monitor->monitor);
}

JNIEXPORT jint JNICALL 
Java_sun_jvmp_generic_motif_PthreadSynchroObject_doWait(JNIEnv *env, 
							jobject jobj, 
							jlong handle,
							jint milli)
{
  JVMP_Monitor* monitor = (JVMP_Monitor*)JLONG_TO_PTR(handle);
  int r;

  if (monitor->mutex_state != JVMP_STATE_LOCKED)
      return JVMP_ERROR_MUTEX_NOT_LOCKED;
  if (milli == 0) 
    r = pthread_cond_wait((pthread_cond_t*)monitor->monitor, 
			  (pthread_mutex_t*)monitor->mutex);
  else
    {
      struct timespec abstime;
      jlong end;
      
      end = getTimeMillis() + milli;
      abstime.tv_sec = end / 1000;
      abstime.tv_nsec = (end % 1000) * 1000000;
      r = pthread_cond_timedwait((pthread_cond_t*)monitor->monitor, 
				 (pthread_mutex_t*)monitor->mutex, 
				 &abstime);
    }
  switch (r)
    {
    case 0:
      return 0; /* OK */
    case EINTR:
      return -1; /* interrupted */
    case ETIMEDOUT:
      return 0; /* OK as far */
    default:
      return -2;
    }
}

