package com.netscape.jsdebugging.remote.corba;
public class _st_IJSExecutionHook extends org.omg.CORBA.portable.ObjectImpl implements com.netscape.jsdebugging.remote.corba.IJSExecutionHook {
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:IJSExecutionHook:1.0"
  };
  public void aboutToExecute(
    com.netscape.jsdebugging.remote.corba.IJSThreadState arg0,
    com.netscape.jsdebugging.remote.corba.IJSPC arg1
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("aboutToExecute", true);
      com.netscape.jsdebugging.remote.corba.IJSThreadStateHelper.write(_output, arg0);
      com.netscape.jsdebugging.remote.corba.IJSPCHelper.write(_output, arg1);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      aboutToExecute(
        arg0,
        arg1
      );
    }
  }
}
