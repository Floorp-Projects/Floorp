package com.netscape.jsdebugging.remote.corba;
public class _st_StringReciever extends org.omg.CORBA.portable.ObjectImpl implements com.netscape.jsdebugging.remote.corba.StringReciever {
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:StringReciever:1.0"
  };
  public void recieveString(
    java.lang.String arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("recieveString", true);
      _output.write_string(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      recieveString(
        arg0
      );
    }
  }
  public void bounce(
    int arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("bounce", true);
      _output.write_long(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      bounce(
        arg0
      );
    }
  }
}
