package com.netscape.jsdebugging.remote.corba;
public interface IJSExecutionHook extends org.omg.CORBA.Object {
  public void aboutToExecute(
    com.netscape.jsdebugging.remote.corba.IJSThreadState arg0,
    com.netscape.jsdebugging.remote.corba.IJSPC arg1
  );
}
