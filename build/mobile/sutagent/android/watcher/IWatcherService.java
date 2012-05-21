/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Original file: C:\\Users\\Bob\\workspace\\Watcher\\src\\com\\mozilla\\watcher\\IWatcherService.aidl
 */
package com.mozilla.watcher;
public interface IWatcherService extends android.os.IInterface
{
/** Local-side IPC implementation stub class. */
public static abstract class Stub extends android.os.Binder implements com.mozilla.watcher.IWatcherService
{
private static final java.lang.String DESCRIPTOR = "com.mozilla.watcher.IWatcherService";
/** Construct the stub at attach it to the interface. */
public Stub()
{
this.attachInterface(this, DESCRIPTOR);
}
/**
 * Cast an IBinder object into an com.mozilla.watcher.IWatcherService interface,
 * generating a proxy if needed.
 */
public static com.mozilla.watcher.IWatcherService asInterface(android.os.IBinder obj)
{
if ((obj==null)) {
return null;
}
android.os.IInterface iin = (android.os.IInterface)obj.queryLocalInterface(DESCRIPTOR);
if (((iin!=null)&&(iin instanceof com.mozilla.watcher.IWatcherService))) {
return ((com.mozilla.watcher.IWatcherService)iin);
}
return new com.mozilla.watcher.IWatcherService.Stub.Proxy(obj);
}
public android.os.IBinder asBinder()
{
return this;
}
@Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
{
switch (code)
{
case INTERFACE_TRANSACTION:
{
reply.writeString(DESCRIPTOR);
return true;
}
case TRANSACTION_UpdateApplication:
{
data.enforceInterface(DESCRIPTOR);
java.lang.String _arg0;
_arg0 = data.readString();
java.lang.String _arg1;
_arg1 = data.readString();
java.lang.String _arg2;
_arg2 = data.readString();
int _arg3;
_arg3 = data.readInt();
int _result = this.UpdateApplication(_arg0, _arg1, _arg2, _arg3);
reply.writeNoException();
reply.writeInt(_result);
return true;
}
}
return super.onTransact(code, data, reply, flags);
}
private static class Proxy implements com.mozilla.watcher.IWatcherService
{
private android.os.IBinder mRemote;
Proxy(android.os.IBinder remote)
{
mRemote = remote;
}
public android.os.IBinder asBinder()
{
return mRemote;
}
public java.lang.String getInterfaceDescriptor()
{
return DESCRIPTOR;
}
public int UpdateApplication(java.lang.String sPkgName, java.lang.String sPkgFileName, java.lang.String sOutFile, int bReboot) throws android.os.RemoteException
{
android.os.Parcel _data = android.os.Parcel.obtain();
android.os.Parcel _reply = android.os.Parcel.obtain();
int _result;
try {
_data.writeInterfaceToken(DESCRIPTOR);
_data.writeString(sPkgName);
_data.writeString(sPkgFileName);
_data.writeString(sOutFile);
_data.writeInt(bReboot);
mRemote.transact(Stub.TRANSACTION_UpdateApplication, _data, _reply, 0);
_reply.readException();
_result = _reply.readInt();
}
finally {
_reply.recycle();
_data.recycle();
}
return _result;
}
}
static final int TRANSACTION_UpdateApplication = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
}
public int UpdateApplication(java.lang.String sPkgName, java.lang.String sPkgFileName, java.lang.String sOutFile, int bReboot) throws android.os.RemoteException;
}
