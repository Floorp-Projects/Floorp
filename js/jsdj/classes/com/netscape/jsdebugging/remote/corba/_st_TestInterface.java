package com.netscape.jsdebugging.remote.corba;
public class _st_TestInterface extends org.omg.CORBA.portable.ObjectImpl implements com.netscape.jsdebugging.remote.corba.TestInterface {
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:TestInterface:1.0"
  };
  public java.lang.String getFirstAppInList() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getFirstAppInList", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      java.lang.String _result;
      _result = _input.read_string();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getFirstAppInList();
    }
  }
  public void getAppNames(
    com.netscape.jsdebugging.remote.corba.StringReciever arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getAppNames", true);
      com.netscape.jsdebugging.remote.corba.StringRecieverHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      getAppNames(
        arg0
      );
    }
  }
  public com.netscape.jsdebugging.remote.corba.Thing[] getThings() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getThings", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.Thing[] _result;
      _result = com.netscape.jsdebugging.remote.corba.TestInterfacePackage.sequence_of_ThingHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getThings();
    }
  }
  public void callBounce(
    com.netscape.jsdebugging.remote.corba.StringReciever arg0,
    int arg1
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("callBounce", true);
      com.netscape.jsdebugging.remote.corba.StringRecieverHelper.write(_output, arg0);
      _output.write_long(arg1);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      callBounce(
        arg0,
        arg1
      );
    }
  }
}
