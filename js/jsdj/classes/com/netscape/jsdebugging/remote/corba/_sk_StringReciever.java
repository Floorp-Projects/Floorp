package com.netscape.jsdebugging.remote.corba;
abstract public class _sk_StringReciever extends org.omg.CORBA.portable.Skeleton implements com.netscape.jsdebugging.remote.corba.StringReciever {
  protected _sk_StringReciever(java.lang.String name) {
    super(name);
  }
  protected _sk_StringReciever() {
    super();
  }
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:StringReciever:1.0"
  };
  public org.omg.CORBA.portable.MethodPointer[] _methods() {
    org.omg.CORBA.portable.MethodPointer[] methods = {
      new org.omg.CORBA.portable.MethodPointer("recieveString", 0, 0),
      new org.omg.CORBA.portable.MethodPointer("bounce", 0, 1),
    };
    return methods;
  }
  public boolean _execute(org.omg.CORBA.portable.MethodPointer method, org.omg.CORBA.portable.InputStream input, org.omg.CORBA.portable.OutputStream output) {
    switch(method.interface_id) {
    case 0: {
      return com.netscape.jsdebugging.remote.corba._sk_StringReciever._execute(this, method.method_id, input, output); 
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
  public static boolean _execute(com.netscape.jsdebugging.remote.corba.StringReciever _self, int _method_id, org.omg.CORBA.portable.InputStream _input, org.omg.CORBA.portable.OutputStream _output) {
    switch(_method_id) {
    case 0: {
      java.lang.String arg0;
      arg0 = _input.read_string();
      _self.recieveString(arg0);
      return false;
    }
    case 1: {
      int arg0;
      arg0 = _input.read_long();
      _self.bounce(arg0);
      return false;
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
}
