package com.netscape.jsdebugging.remote.corba;
public class _st_IScriptHook extends org.omg.CORBA.portable.ObjectImpl implements com.netscape.jsdebugging.remote.corba.IScriptHook {
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:IScriptHook:1.0"
  };
  public void justLoadedScript(
    com.netscape.jsdebugging.remote.corba.IScript arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("justLoadedScript", true);
      com.netscape.jsdebugging.remote.corba.IScriptHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      justLoadedScript(
        arg0
      );
    }
  }
  public void aboutToUnloadScript(
    com.netscape.jsdebugging.remote.corba.IScript arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("aboutToUnloadScript", true);
      com.netscape.jsdebugging.remote.corba.IScriptHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      aboutToUnloadScript(
        arg0
      );
    }
  }
}
