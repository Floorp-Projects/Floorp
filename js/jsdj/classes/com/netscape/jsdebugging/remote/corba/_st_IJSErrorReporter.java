package com.netscape.jsdebugging.remote.corba;
public class _st_IJSErrorReporter extends org.omg.CORBA.portable.ObjectImpl implements com.netscape.jsdebugging.remote.corba.IJSErrorReporter {
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:IJSErrorReporter:1.0"
  };
  public int reportError(
    java.lang.String arg0,
    java.lang.String arg1,
    int arg2,
    java.lang.String arg3,
    int arg4
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("reportError", true);
      _output.write_string(arg0);
      _output.write_string(arg1);
      _output.write_long(arg2);
      _output.write_string(arg3);
      _output.write_long(arg4);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      int _result;
      _result = _input.read_long();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return reportError(
        arg0,
        arg1,
        arg2,
        arg3,
        arg4
      );
    }
  }
}
