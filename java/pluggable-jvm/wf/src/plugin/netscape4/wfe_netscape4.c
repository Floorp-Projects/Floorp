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
 * $Id: wfe_netscape4.c,v 1.2 2001/07/12 19:58:22 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include "jvmp.h"
#include "common.h"
#include "shmtran.h"

/* Extension parameters */
static jint  g_side = 0;

#ifdef XP_UNIX
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdlib.h>

/* PART 1 - application specific host callbacks */
static XID      g_xid  = 0;
static int      g_msgid1 = -1;
static Display* GetDisplay()
{
  char* dname;
  static Display* dpy = NULL;
  
  if (dpy) return dpy;
  dname = getenv("DISPLAY");
  if (!dname) return NULL; 
  dpy = XOpenDisplay(dname);
  return dpy;
}

static int SetTransport(int xid, int* pmsg_id)
{
  int msg_id;

  g_xid = xid;
  /* create msg queue used by extension to recieve messages */
  if ((msg_id = 
       JVMP_msgget (0, IPC_CREAT | IPC_PRIVATE | IPC_EXCL | 0770)) == -1)
    {
      perror("host: msgget");
      return 0;
    }
  *pmsg_id = msg_id;
  return 1;
}
#endif

int NotifyHost()
{
#ifdef XP_UNIX
  static Atom     atom = 0;
  static Display* dpy;
  
  if (g_side != 2 || !g_xid) return 0; /* has sense only on JVM side 
				        and after init message has come */
  dpy = GetDisplay();
  if (!dpy) return 0;
  if (!atom) atom = XInternAtom(dpy, "WF atom", FALSE);
  XChangeProperty(dpy, g_xid, atom, XA_STRING, 8,
		  PropModeReplace, NULL, 0);
  XFlush(dpy);
  return 1;
#endif
}


/* PART 2 - Extension API implementation and related functions */

/* this is a dummy method to make linker happy */
int JVMP_ExecuteShmRequest(JVMP_ShmRequest* req)
{
  fprintf(stderr, "NEVER CALL ME\n");
  return 1;
}

/* this one is called only on JVM side and handle extension spec*/
static int JVMP_ExecuteExtShmRequest(JVMP_ShmRequest* req)
{
  int vendor, funcno, xid;
  char sign[40], *docbase;
  int* pmsg_id;
  jint *pid;
  
  if (!req) return 0;
  vendor = JVMP_GET_VENDOR(req->func_no);
  if (vendor != WFNetscape4VendorID) return 0;
  funcno = JVMP_GET_EXT_FUNCNO(req->func_no);
  switch(funcno)
    {
    case 1:
      /* XID setter on Unix, HANDLE setter on Win32 */
      sprintf(sign, "Ii");
      pmsg_id = &g_msgid1;
      JVMP_DecodeRequest(req, 0, sign, &xid, &pmsg_id);
      /* kinda tricky to allow msg queue clean up */
      req->retval = SetTransport(xid, &g_msgid1);
      JVMP_EncodeRequest(req, sign, xid, pmsg_id);
      break;
    case 5:
      /* doc base setter - stupid Netscape */
      sprintf(sign, "IS");
      pid = NULL;
      JVMP_DecodeRequest(req, 1, sign, pid, &docbase);
      fprintf(stderr, "got docbase %s for id %ld\n", docbase, *pid);
      free(pid); free(docbase);
      break;
    default:
      fprintf(stderr, "JVM got func %d\n", funcno);
      return 0;
    }
  return 1;
}


/* extension API implementation */
static jint JNICALL JVMPExt_Init(jint side)
{
  g_side = side;
  if (g_side == 0) return JNI_TRUE; /* in-proc  case. Do nothing yet */
  if (g_side == 1) 
    {
      /* host side - create event widget and custom message queue, 
         and send their XID and msg_id to JVM side using
	 SHM transport. Sync is OK, as this Init methods are called in right
         order - host side first, JVM side next. Really I'll do it
	 in JavaPlugin.c due current design it's more acceptable. */
    }
  else
    {
      /* JVM side - add extention callback to handle XID when it'll come.
	 Then use this XID as notificator after putting smth into this 
	 extension's SHM queue. It's also not required as now callback 
	 is JVMPExt_ScheduleRequest and called by JVMP automatically on every 
	 non system request.
      */ 
    }
  return JNI_TRUE;
}

static jint JNICALL JVMPExt_Shutdown()
{
  return JNI_TRUE;
}

static jint JNICALL JVMPExt_GetExtInfo(jint *pID, jint *pVersion)
{
  *pID = WFNetscape4VendorID;
  *pVersion = WFNetscape4ExtVersion;
  return JNI_TRUE;
}


static jint JNICALL JVMPExt_GetBootstrapClass(char* *bootstrapClassPath,
					      char* *bootstrapClassName)
{
  /* should be defined at installation time */
  *bootstrapClassPath = 
    "file:///ws/M2308/mozilla/modules/jvmp/build/unix/classes/";
  *bootstrapClassName = 
    "sun.jvmp.netscape4.NetscapePeerFactory";
  return JNI_TRUE;
}

static jint JNICALL JVMPExt_ScheduleRequest(JVMP_ShmRequest* req, 
					    jint local)
{
  if (local == JNI_TRUE) 
      return JVMP_ExecuteExtShmRequest(req);
  if (g_side == 1) 
    {
      /* host side */
      JVMP_SendShmRequest(req, 1);
      JVMP_WaitConfirmShmRequest(req);
    }
  else
    {
      /* JVM side */      
      JVMP_SendShmRequest(req, 1);      
      if (NotifyHost()) JVMP_WaitConfirmShmRequest(req);
    }
  return JNI_TRUE;
}



static JVMP_Extension JVMP_Ext = 
{
    &JVMPExt_Init,
    &JVMPExt_Shutdown,
    &JVMPExt_GetExtInfo,
    &JVMPExt_GetBootstrapClass,
    &JVMPExt_ScheduleRequest
};

jint JNICALL JVMP_GetExtension(JVMP_Extension** ext)
{
    *ext = &JVMP_Ext;
    return JNI_TRUE;
}
/* PART3 - Java native methods implementation */

jint getID(JNIEnv* env, jobject jobj) 
{
  jint id;
  static jfieldID peerFID = NULL;
  
  if (!peerFID) 
    {
      peerFID = (*env)->GetFieldID(env,
				   (*env)->GetObjectClass(env, jobj),
				   "m_id","I");
      if (!peerFID) return 0;
    }
  id = (*env)->GetIntField(env, jobj, peerFID);
  return id;
}

static jint  GetParameters(jint id, char*** pkeys, char*** pvals, int *plen)
{
  JVMP_ShmRequest* req;
  int *pres, *pid, i, len, argc, pos;
  char* buf, sign[20];
  char **keys, **vals;
#define EXTRACT_STRING(res) \
     len = strlen(buf+pos)+1; \
     res = (char*)malloc(len); \
     memcpy(res, buf+pos, len); \
     pos += len;

  req = JVMP_NewShmReq(g_msgid1, 
		       JVMP_NEW_EXT_FUNCNO(WFNetscape4VendorID, 2));
  pres = NULL;
  pid = NULL;
  buf = NULL;
  sprintf(sign, "Iia[0]");
  JVMP_EncodeRequest(req, sign, id, pres, buf);
  JVMPExt_ScheduleRequest(req, JNI_FALSE);
  JVMP_DecodeRequest(req, 1, sign, pid, &pres, &buf);
  /* here we hopefully got all our parameters in one huge buffer 
     now is time to make strings arrays from 'em */
  argc = *pres;
  keys = (char**)malloc(argc*sizeof(char*));
  vals = (char**)malloc(argc*sizeof(char*));
  for (i=0, pos=0; i<argc; i++)
    {
      EXTRACT_STRING(keys[i]);
      EXTRACT_STRING(vals[i]);
    }
  free(buf); free(pres); free(pid);
  JVMP_DeleteShmReq(req);
  *pkeys = keys;
  *pvals = vals;
  *plen = argc;
  return JNI_TRUE;
} 

static jint ShowStatus(jint id, char* status)
{
  JVMP_ShmRequest* req;
  char sign[40];
  
  req = JVMP_NewShmReq(g_msgid1, 
		       JVMP_NEW_EXT_FUNCNO(WFNetscape4VendorID, 3));
  sprintf(sign, "IS");
  JVMP_EncodeRequest(req, sign, id, status);
  JVMPExt_ScheduleRequest(req, JNI_FALSE);
  JVMP_DeleteShmReq(req);
  return req->retval;
}

static jint ShowDocument(jint id, char* url, char* status)
{
  JVMP_ShmRequest* req;
  char sign[40];
  
  req = JVMP_NewShmReq(g_msgid1, 
		       JVMP_NEW_EXT_FUNCNO(WFNetscape4VendorID, 4));
  sprintf(sign, "ISS");
  JVMP_EncodeRequest(req, sign, id, url, status);
  JVMPExt_ScheduleRequest(req, JNI_FALSE);
  JVMP_DeleteShmReq(req);
  return req->retval;
}



JNIEXPORT jobjectArray JNICALL 
Java_sun_jvmp_netscape4_MozillaAppletPeer_getParams(JNIEnv* env, jobject jobj)
{
  char **keys, **vals;
  unsigned int i, len;
  jclass StringClass, StringArrayClass;
  jobjectArray jkeys, jvals, result;
  jint id;
  
  id = getID(env, jobj); 
  if (!GetParameters(id, &keys, &vals, &len)) return NULL;
  // hopefully here runtime is inited so those classes resolved 
  // without problems - maybe cache it
  StringClass = (*env)->FindClass(env, "java/lang/String");
  StringArrayClass = (*env)->FindClass(env, "[Ljava/lang/String;");
  result = (*env)->NewObjectArray(env, 2, StringArrayClass, NULL); 
  jkeys = (*env)->NewObjectArray(env, len, StringClass, NULL);
  jvals = (*env)->NewObjectArray(env, len, StringClass, NULL);
  for (i=0; i<len; i++) 
      {
	  (*env)->SetObjectArrayElement(env, jkeys, i, 
					(*env)->NewStringUTF(env, keys[i]));
	  (*env)->SetObjectArrayElement(env, jvals, i, 
					(*env)->NewStringUTF(env, vals[i]));
	  free(keys[i]); free(vals[i]);
      }
  free(keys); free(vals);
  (*env)->SetObjectArrayElement(env, result, 0, jkeys);
  (*env)->SetObjectArrayElement(env, result, 1, jvals);
  (*env)->DeleteLocalRef(env, jkeys);
  (*env)->DeleteLocalRef(env, jvals);
  return result;
}

#if 0
JNIEXPORT jstring JNICALL 
Java_sun_jvmp_netscape4_MozillaAppletPeer_getProxyForURL(JNIEnv * env, 
						       jobject jobj, 
						       jstring jurl)
{
    jstring jresult;
    jint res;
    char  *url, *result;
    PluginInstance *wrapper;

    if (!(wrapper = getWrapper(env, jobj))) return NULL; 
    url = (char*)(*env)->GetStringUTFChars(env, jurl, NULL);       
    res = wrapper->GetProxyForURL(wrapper->info, url, &result);
    (*env)->ReleaseStringUTFChars(env, jurl, url);
    if (res != 0) return NULL;
    jresult = (*env)->NewStringUTF(env, result);
    return jresult;
}
#endif
/*
 * Class:     sun_jvmp_netscape4_MozillaAppletPeer
 * Method:    nativeShowStatus
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL 
Java_sun_jvmp_netscape4_MozillaAppletPeer_nativeShowStatus
  (JNIEnv * env, jobject jobj, jstring jstatus)
{
    jint res;
    char  *status;
    jint id;

    if (!(id = getID(env, jobj))) return JNI_FALSE; 
    status = (char*)(*env)->GetStringUTFChars(env, jstatus, NULL);       
    res = ShowStatus(id, status);
    (*env)->ReleaseStringUTFChars(env, jstatus, status);
    return (res == 0);
}

/*
 * Class:     sun_jvmp_netscape4_MozillaAppletPeer
 * Method:    nativeShowDocument
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL 
Java_sun_jvmp_netscape4_MozillaAppletPeer_nativeShowDocument
  (JNIEnv *env, jobject jobj, jstring jurl, jstring jtarget)
{
    int res;
    char  *url, *target;
    jint id;

    if (!(id = getID(env, jobj))) return JNI_FALSE; 
    url = (char*)(*env)->GetStringUTFChars(env, jurl, NULL);
    target = (char*)(*env)->GetStringUTFChars(env, jtarget, NULL);    
    res = ShowDocument(id, url, target);
    (*env)->ReleaseStringUTFChars(env, jurl, url);
    (*env)->ReleaseStringUTFChars(env, jtarget, target);
    return (res == 0);
}





