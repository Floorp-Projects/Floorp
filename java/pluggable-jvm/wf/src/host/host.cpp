/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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
 * $Id: host.cpp,v 1.2 2001/07/12 19:58:14 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include <stdio.h>
#include <jvmp.h>
#include <string.h>
#include <stdlib.h> // to getenv()
#include <gtk/gtk.h>

#ifdef XP_UNIX
#include <unistd.h> // to sleep()
#include <pthread.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <gdk/gdkx.h>
#define MUTEX_T          pthread_mutex_t
#define MUTEX_CREATE(m)  m = (MUTEX_T*)malloc(sizeof(MUTEX_T)); pthread_mutex_init(m, NULL)
#define MUTEX_LOCK(m)    pthread_mutex_lock(m)
#define MUTEX_UNLOCK(m)  pthread_mutex_unlock(m)   
#define COND_T           pthread_cond_t
#define COND_CREATE(c)   c = (COND_T*)malloc(sizeof(COND_T)); pthread_cond_init(c, NULL) 
#define dll_t   void*
#define DL_MODE RTLD_NOW
#define FILE_SEPARATOR "/"
#define PATH_SEPARATOR ":"
#define DL_MODE RTLD_NOW
// gcc specific
#define DEBUG(x...) fprintf(stderr, "host: "x)
#endif

#ifdef XP_WIN32
#include <windows.h>
#include <gdk\win32\gdkwin32.h>
#define MUTEX_T          CRITICAL_SECTION
#define dll_t HINSTANCE
#define DL_MODE 0
#define FILE_SEPARATOR "\\"
#define PATH_SEPARATOR ";"
#define DL_MODE 0
#define DEBUG 
#endif


JVMP_GetPlugin_t fJVMP_GetPlugin;

// XXX: JVMP_CallingContext is per-thread structure, but almost all calls here
// goes from the "main" GTK thread.
JVMP_CallingContext* ctx = NULL;
JavaVM*              jvm = NULL;
JNIEnv*              env = NULL;
JavaVMInitArgs       vm_args;
GtkWidget*           topLevel = NULL;
JVMP_RuntimeContext* jvmp_context = NULL;
jint                 g_wid = 0; // ID of topLevel inside JVM
jint                 g_oid = 0; // ID of Java peer object 
jint                 g_eid = 0; // ID of test extension
jint                 g_mid = 0; // ID of monitor object 
char                 g_errbuf[JVMP_MAXERRORLENGTH];
#ifdef XP_UNIX // yet
MUTEX_T*             g_mutex = NULL;
COND_T*              g_cond  = NULL;
JVMP_Monitor*        g_mon   = NULL;
#endif


#ifdef XP_UNIX
dll_t LoadDLL(const char* filename, int mode)
{
  return dlopen(filename, mode);
}

void* FindSymbol(dll_t dll, const char* sym)
{
  return dlsym(dll, sym);
}

int   UnloadDLL(dll_t dll)
{
  return dlclose(dll);
}

const char* LastDLLErrorString()
{
  return dlerror();
}

#endif
#ifdef XP_WIN32
dll_t LoadDLL(const char* filename, int mode)
{
  return LoadLibrary(filename);
}

void* FindSymbol(dll_t dll, const char* sym)
{
  return GetProcAddress(dll, sym);
}

int   UnloadDLL(dll_t dll)
{
  // XXX: fix me
  return 0;
}

const char*  LastDLLErrorString()
{
  static LPVOID lpMsgBuf = NULL;
  
  free(lpMsgBuf);
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |     
                FORMAT_MESSAGE_IGNORE_INSERTS,    
                NULL,
                GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf, 0, NULL ); 
  return (const char*)lpMsgBuf;
}
#endif

jint loadPluginLibrary(void** h) {
  dll_t handle;
  char *path, *home;
#ifdef XP_UNIX
#ifdef JVMP_USE_SHM
  char* filename="libjvmp_shm.so";
#else
  char* filename="libjvmp.so";
#endif
#endif
#ifdef XP_WIN32
  char* filename="jvmp.dll";
#endif
  home = getenv("WFHOME");
  if (!home) return JNI_FALSE;
  path = (char*)malloc(strlen(home)+strlen(filename)+2);
  sprintf(path, "%s"FILE_SEPARATOR"%s", home, filename);
  handle = LoadDLL(path, DL_MODE);
  if (!handle) {
    fprintf(stderr, "dlopen: %s\n", LastDLLErrorString());
    return JNI_FALSE;
  };
  
  fJVMP_GetPlugin = 
    (JVMP_GetPlugin_t)FindSymbol(handle, "JVMP_GetPlugin");
  if (!fJVMP_GetPlugin)  {
    fprintf(stderr, "dlsym:%s\n", LastDLLErrorString());
    return JNI_FALSE;
  };
  if ((*fJVMP_GetPlugin)(&jvmp_context) != JNI_TRUE) 
    {
      fprintf(stderr, "JVMP_GetPlugin failed\n");
      return JNI_FALSE;
    }
  (*h) = handle;
  return JNI_TRUE;
}

jint initJVM(JavaVM** jvm, JNIEnv** env, JavaVMInitArgs* pvm_args) {
  pvm_args->version = JNI_VERSION_1_2;
  jint res = (jvmp_context->JVMP_GetRunningJVM)(&ctx, pvm_args, JNI_TRUE);
  if (res == JNI_TRUE) {
    *env = ctx->env;
    *jvm = jvmp_context->jvm;
    fprintf(stderr, "++++++JVM successfully obtained.\n");
    return JNI_TRUE;
  } else {
    fprintf(stderr, "------Failed to obtain JVM.\n");
    return JNI_FALSE;
  };
};


void b1_handler(GtkWidget *widget) 
{
  void* handle;

  if (!loadPluginLibrary(&handle)) return;
  memset(&vm_args, 0, sizeof(vm_args));
  if (!initJVM(&jvm, &env, &vm_args)) {
    return;
  };
};

void b2_handler(GtkWidget *widget) {
  if (!jvmp_context) return;
  if (g_eid) (jvmp_context->JVMP_UnregisterExtension)(ctx, g_eid);
  (jvmp_context->JVMP_StopJVM)(ctx);
};

void b4_handler(GtkWidget *widget) {
  JVMP_DrawingSurfaceInfo w;

  if (!jvmp_context) return;
  jint containerWindowID = (jint) GDK_WINDOW_XWINDOW(topLevel->window);
  w.window =  (JPluginWindow *)containerWindowID;
#ifdef XP_UNIX
  gdk_flush();
#endif
  w.x = 0;
  w.y = 0;
  w.width  = topLevel->allocation.width;
  w.height = topLevel->allocation.height;

  if ((jvmp_context->JVMP_RegisterWindow)(ctx, &w, &g_wid) != JNI_TRUE) {
    jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
    fprintf(stderr, "Can\'t register window: %s\n", g_errbuf);
    return;
  };
  fprintf(stderr, "Registed our GTK window with ID %d\n", (int)g_wid);
}

void b5_handler(GtkWidget *widget) {
    if (!jvmp_context || !g_oid) return;
      if ((jvmp_context->JVMP_SendEvent)
	  (ctx, g_oid, 20, (jlong) g_wid, JVMP_PRIORITY_NORMAL) == JNI_FALSE)
	  {
	      jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
	      fprintf(stderr, "cannot send event to %d: %s\n", 
		      (int)g_oid, g_errbuf);	      
	      return;
	  }
      fprintf(stderr, "Event sent to %d\n", (int)g_oid);  
}

void b6_handler(GtkWidget *widget) {
  if (!jvmp_context) return;
  if ((jvmp_context->JVMP_UnregisterWindow)(ctx, g_wid) != JNI_TRUE) {
    jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
    fprintf(stderr, "Can\'t unregister window: %s\n", g_errbuf);
    return;
  };
  fprintf(stderr, "Unregisted our GTK window with ID %d\n", (int)g_wid);
}

void b9_handler(GtkWidget *widget) {
  if (!jvmp_context) return;
  JVMP_PluginDescription* d;
  if ((jvmp_context->JVMP_GetDescription)(&d) != JNI_TRUE)
    {
      fprintf(stderr, "Cannot get description\n");
      return;
    }
  fprintf(stderr, 
	  "Waterfall plugin info:\n"
	  "JVM vendor=\"%s\"\n"
	  "JVM version=\"%s\"\n"
	  "JVM kind=\"%s\"\n"
	  "JVMP version=\"%s\"\n"
	  "platform=\"%s\"\n"
	  "arch=\"%s\"\n"
	  "vendor data=\"%p\"\n",
	  d->vendor, d->jvm_version, d->jvm_kind, d->jvmp_version,
	  d->platform, d->arch, d->vendor_data);

}
#ifdef XP_UNIX
void* thread_func(void* arg) {
    jint id;
    JVMP_CallingContext* myctx;
    
    if ((jvmp_context->JVMP_GetCallingContext)(&myctx) != JNI_TRUE) 
	return NULL;
    if ((jvmp_context->JVMP_AttachCurrentThread)(myctx, &id) != JNI_TRUE) 
	fprintf(stderr, "Can\'t attach thread\n");
    else 
	fprintf(stderr, "Attached with ID %d\n", (int)id);
    sleep(10);
    // this function MUST be called by any function performed JVMP_GetCallingContext
    // or JVMP_AttachCurrentThread. as otherwise structures inside of Waterfall 
    // not cleaned up. Also it destroys myctx
    (jvmp_context->JVMP_DetachCurrentThread)(myctx);
    return NULL;
}

#endif

void b8_handler(GtkWidget *widget) {
#ifdef XP_UNIX
  pthread_t tid;
  
  if (!jvmp_context) return;
  // allocate in heap to prevent invalid deallocation 
  pthread_create(&tid, NULL, &thread_func, NULL);
  pthread_detach(tid);
#endif
}

void registerMyExtension()
{
#ifdef XP_UNIX
  const char* ext = "./libwf_test.so";
#endif
#ifdef XP_WIN32
  const char* ext = "wf_test.dll";
#endif

  if ((jvmp_context->JVMP_RegisterExtension)
      (ctx, ext, &g_eid, 0) != JNI_TRUE)
	{
	    jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
	    fprintf(stderr, "extension not registred: %s\n", g_errbuf);
	    return;
	}
  fprintf(stderr, "Registered extension with ID: %d\n", (int)g_eid);
  if ((jvmp_context->JVMP_SendExtensionEvent)(ctx, g_eid, 
					      13, 0, JVMP_PRIORITY_NORMAL) == JNI_FALSE)
	{
	    jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
	    fprintf(stderr, "cannot send extension event: %s\n", g_errbuf);
	    return;
	}
  fprintf(stderr, "sent event 13 to newly registred extension\n");
}

void b10_handler(GtkWidget *widget) {
  if (!jvmp_context) return;
  registerMyExtension();
}


int newPrincipalsFromStrings(jbyte** *resprin, jint* *reslen, jint num, ...)
{
    jint*  prins_len; 
    jbyte* *prins;
    char* str;
    jint i, len;
    va_list ap;

    prins_len = (jint*)malloc(num*sizeof(jint));
    if (!prins_len) return 0;
    prins = (jbyte**)malloc(num*sizeof(jbyte**));

    if (!prins) return 0;
    va_start(ap, num);
    for (i=0; i<num; i++) 
	{
	    str =  va_arg(ap, char*);
	    len = str ? strlen(str) : 0;
	    prins[i] = (jbyte*)malloc(len);
	    if (!prins[i]) return 0;
	    prins_len[i] = len;
	    /* not copy last 0 */
	    memcpy(prins[i], str, len);
	}
    va_end(ap);
    *resprin = prins;
    *reslen = prins_len;
    return 1;
}
int deletePrincipals(jbyte** prins, jint* plen, jint len)
{
    int i;
    for (i=0; i<len; i++)
	free(prins[i]);
    free(prins);
    free(plen);
    return 1;
}

void b12_handler(GtkWidget *widget) {
#ifdef XP_UNIX
  static int registered = 0;
  int millis = 20000;
  jlong end;
  struct timeval  tv, tv1;
  struct timespec ts;
  long diff;
 if (!jvmp_context) return;
  if (!registered)
    {
      if ((jvmp_context->JVMP_RegisterMonitorObject)(ctx, g_mon, &g_mid) != JNI_TRUE)
	{
	  DEBUG("couldn\'t register monitor object\n");
	  return;
	}
      registered = 1;
    }
  if (g_oid) {
    // now post message to Java peer with request to start thread that will
    // try to acquire the same mutex and wake up us on common native monitor. 
    // Not perfect, as possible race condition -
    // if event handled too fast, but OK for testing
    (jvmp_context->JVMP_PostEvent)
	(ctx, g_oid, 666, (jlong)g_mid, JVMP_PRIORITY_HIGH);    
  }
  DEBUG("locking mutex\n");
  MUTEX_LOCK(g_mutex);
  DEBUG("LOCKED. Sleeping no more %d millis, waiting until Java thread will wake up us\n", millis);
  gettimeofday(&tv, 0);
  end = ((jlong)tv.tv_sec)*1000 + ((jlong)tv.tv_usec)/1000 + millis;
  ts.tv_sec  = end / 1000;
  ts.tv_nsec = (end % 1000) * 1000000;
  /* pthread_cond_timedwait() reacquires mutex, so just changing
     millis on Java side to value greater than  millis wouldn't
     force this call to complete earlier. */
  pthread_cond_timedwait(g_cond, g_mutex, &ts);
  gettimeofday(&tv1, 0);
  MUTEX_UNLOCK(g_mutex);
  diff = (tv1.tv_sec-tv.tv_sec)*1000 + (tv1.tv_usec-tv.tv_usec)/1000;
  DEBUG("mutex unlocked after %ld millis\n", diff);
#endif
}

void b11_handler(GtkWidget *widget) {
  static int onoff = 0;  
  
  if (!jvmp_context) return;
  onoff = !onoff;
  if (onoff)
      {
	  if ((jvmp_context->JVMP_CreatePeer)(ctx, 
					      "@sun.com/wf/tests/testextension;1", 
					      0, 
					      &g_oid) != JNI_TRUE)
	      {
		  jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
		  fprintf(stderr, 
			  "Cannot create new peer using this extension: %s\n",
			  g_errbuf);
		  return;
	      }
	  fprintf(stderr, "created peer witd id %d\n", (int)g_oid); 
      }
  else
      {
	  if ((jvmp_context->JVMP_DestroyPeer)(ctx, g_oid) != JNI_TRUE)
	      {
		  jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
		  fprintf(stderr, 
			  "Cannot remove peer: %s\n", g_errbuf);
		  return;
	      }
	  fprintf(stderr, "removed peer witd id %d\n", (int)g_oid);
      }
}

void b13_handler(GtkWidget *widget) {
  static int onoff = 0;  
  
  if (!jvmp_context) return;
  onoff = (onoff == 0) ? 1 : 0;
  if ((jvmp_context->JVMP_PostSysEvent)
      (ctx, JVMP_CMD_CTL_CONSOLE, (jlong)onoff, JVMP_PRIORITY_NORMAL) == JNI_FALSE)
    {
      jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
      fprintf(stderr, 
	      "Cannot send system event: %s\n", g_errbuf);
      return;
    }
}

void b7_handler(GtkWidget *widget) {
  JVMP_SecurityCap cap;
  static int onoff = 0;  
  
  if (!jvmp_context) return;
  onoff = !onoff;
  JVMP_FORBID_ALL_CAPS(cap);
  JVMP_ALLOW_CAP(cap, JVMP_CAP_SYS_SYSTEM);
  JVMP_ALLOW_CAP(cap, JVMP_CAP_SYS_EVENT);
  JVMP_ALLOW_CAP(cap, JVMP_CAP_SYS_SYSEVENT);
  if (onoff)
      {
	  jbyte** prins;
	  jint    pnum = 2;
	  jint*   plen;
	  if (!newPrincipalsFromStrings(&prins, &plen, 
					pnum, "CAPS", "DUMMY"))
	    return;
	  if ((jvmp_context->JVMP_EnableCapabilities)
	      (ctx, &cap, pnum, plen, prins) != JNI_TRUE) 
	      {
		  jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
		  fprintf(stderr, "Can\'t enable caps: %s\n", g_errbuf);
		  return;
	      };
	  deletePrincipals(prins, plen, pnum);
      }

  else
      {
	  if ((jvmp_context->JVMP_DisableCapabilities)
	      (ctx, &cap) != JNI_TRUE) 
	      {
		  jvmp_context->JVMP_GetLastErrorString(ctx, g_errbuf);
		  fprintf(stderr, "Can\'t disable cap: %s\n", g_errbuf);
		  return;
	      };
      }
      
  fprintf(stderr, "Capabilities JVMP_CAP_SYS_SYSTEM and JVMP_CAP_SYS_EVENT triggered to %s\n",
	  onoff ? "ON" : "OFF");
}



GtkWidget*      initGTKStuff(int *argc, char ***argv) {
  //  fprintf(stderr,"explicit threads init %d\n", XInitThreads());
  gtk_init(argc, argv);
  /* Create a new window */
  GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Java Plugin Host");

  /* Here we connect the "destroy" event to a signal handler */ 
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
                      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  gtk_widget_set_usize(window, 300, 400);
  /* Sets the border width of the window. */
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  
  /* Create a Frame */
  GtkWidget* frame = gtk_frame_new(NULL);
   /* Set the style of the frame */
  gtk_frame_set_shadow_type( GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
  gtk_container_add(GTK_CONTAINER(window), frame);
  /////////
  gtk_signal_connect(GTK_OBJECT(window), "destroy",
                     GTK_SIGNAL_FUNC(gtk_main_quit),
                     NULL);
  gtk_signal_connect(GTK_OBJECT(window), "delete-event",
                     GTK_SIGNAL_FUNC(gtk_false),
                     NULL);
  /* Button initialization */
  GtkWidget* button1 = gtk_button_new_with_label ("Start JVM");
  GtkWidget* button2 = gtk_button_new_with_label ("Stop JVM");
  GtkWidget* button3 = gtk_button_new_with_label ("Close application");
  GtkWidget* button4 = gtk_button_new_with_label ("Register native window");
  GtkWidget* button5 = gtk_button_new_with_label ("Test registered window");
  GtkWidget* button6 = gtk_button_new_with_label ("Unregister window");
  GtkWidget* button7 = gtk_button_new_with_label ("Enable/Disable system capabilities");
  GtkWidget* button8 = gtk_button_new_with_label ("Attach new native thread");
  GtkWidget* button9 = gtk_button_new_with_label ("Get WF description");  
  GtkWidget* button10 = gtk_button_new_with_label ("Register test extension");   
  GtkWidget* button11 = gtk_button_new_with_label ("Create/Remove test peer");
  GtkWidget* button12 = gtk_button_new_with_label ("Test Java/native locking ability");
  GtkWidget* button13 = gtk_button_new_with_label ("Show/hide Java console");
  

  gtk_signal_connect (GTK_OBJECT (button1), "clicked",
                        GTK_SIGNAL_FUNC (b1_handler), NULL);
  gtk_signal_connect (GTK_OBJECT (button2), "clicked",
                        GTK_SIGNAL_FUNC (b2_handler), NULL);  
  gtk_signal_connect_object (GTK_OBJECT (button3), "clicked",
                             GTK_SIGNAL_FUNC (gtk_main_quit),
                             GTK_OBJECT (window));
  gtk_signal_connect_object (GTK_OBJECT (button4), "clicked",
                             GTK_SIGNAL_FUNC (b4_handler),
                             GTK_OBJECT (window));  
  gtk_signal_connect_object (GTK_OBJECT (button5), "clicked",
                             GTK_SIGNAL_FUNC (b5_handler),
                             GTK_OBJECT (window));
  gtk_signal_connect_object (GTK_OBJECT (button6), "clicked",
                             GTK_SIGNAL_FUNC (b6_handler),
                             GTK_OBJECT (window));
  gtk_signal_connect_object (GTK_OBJECT (button7), "clicked",
                             GTK_SIGNAL_FUNC (b7_handler),
                             GTK_OBJECT (window));
  gtk_signal_connect_object (GTK_OBJECT (button8), "clicked",
                             GTK_SIGNAL_FUNC (b8_handler),
                             GTK_OBJECT (window));
  gtk_signal_connect_object (GTK_OBJECT (button9), "clicked",
                             GTK_SIGNAL_FUNC (b9_handler),
                             GTK_OBJECT (window));
  gtk_signal_connect_object (GTK_OBJECT (button10), "clicked",
                             GTK_SIGNAL_FUNC (b10_handler),
                             GTK_OBJECT (window));
  gtk_signal_connect_object (GTK_OBJECT (button11), "clicked",
                             GTK_SIGNAL_FUNC (b11_handler),
                             GTK_OBJECT (window));
  gtk_signal_connect_object (GTK_OBJECT (button12), "clicked",
                             GTK_SIGNAL_FUNC (b12_handler),
                             GTK_OBJECT (window));
  gtk_signal_connect_object (GTK_OBJECT (button13), "clicked",
                             GTK_SIGNAL_FUNC (b13_handler),
                             GTK_OBJECT (window));
  GtkWidget* vbox1 = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox1), button1,  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), button2,  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), button13, TRUE, TRUE, 0);  
  gtk_box_pack_start (GTK_BOX (vbox1), button10, TRUE, TRUE, 0);  
  gtk_box_pack_start (GTK_BOX (vbox1), button4,  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), button6,  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), button7,  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), button11, TRUE, TRUE, 0);  
  gtk_box_pack_start (GTK_BOX (vbox1), button5,  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), button8,  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), button9,  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), button12, TRUE, TRUE, 0);  
  gtk_box_pack_start (GTK_BOX (vbox1), button3,  TRUE, TRUE, 0);  
  gtk_container_add(GTK_CONTAINER(frame), vbox1);
  ///////////
  gtk_widget_show(button1);
  gtk_widget_show(button2);
  gtk_widget_show(button3);
  gtk_widget_show(button4);
  gtk_widget_show(button5);
  gtk_widget_show(button6);
  gtk_widget_show(button7);
  gtk_widget_show(button8);
  gtk_widget_show(button9);
  gtk_widget_show(button10);
  gtk_widget_show(button11);
  gtk_widget_show(button12);
  gtk_widget_show(button13);  
  gtk_widget_show(vbox1);
  gtk_widget_show(frame);
  /* Display the window */
  gtk_widget_show (window);

  /* another window to play  with JDK */
  GtkWidget* window1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_usize(window1, 300, 300);
  GtkWidget* frame1 = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type( GTK_FRAME(frame1), GTK_SHADOW_ETCHED_OUT);
  gtk_container_add(GTK_CONTAINER(window1), frame1);
  gtk_widget_show (frame1);
  gtk_widget_show (window1);
  topLevel = window1;
  return window1;
}

int main(int argc, char** argv) {
  initGTKStuff(&argc, &argv);
// this stuff works only on unix yet
#ifdef XP_UNIX
  MUTEX_CREATE(g_mutex);
  COND_CREATE(g_cond);
  JVMP_Monitor mon = JVMP_MONITOR_INITIALIZER(g_mutex, g_cond);
  g_mon = &mon;
#endif
  gtk_main();
  return 0;
};










