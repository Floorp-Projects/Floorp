package com.netscape.jsdebugging.remote.corba;
abstract public class _sk_IJSErrorReporter extends org.omg.CORBA.portable.Skeleton implements com.netscape.jsdebugging.remote.corba.IJSErrorReporter {
  protected _sk_IJSErrorReporter(java.lang.String name) {
    super(name);
  }
  protected _sk_IJSErrorReporter() {
    super();
  }
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:IJSErrorReporter:1.0"
  };
  public org.omg.CORBA.portable.MethodPointer[] _methods() {
    org.omg.CORBA.portable.MethodPointer[] methods = {
      new org.omg.CORBA.portable.MethodPointer("reportError", 0, 0),
    };
    return methods;
  }
  public boolean _execute(org.omg.CORBA.portable.MethodPointer method, org.omg.CORBA.portable.InputStream input, org.omg.CORBA.portable.OutputStream output) {
    switch(method.interface_id) {
    case 0: {
      return com.netscape.jsdebugging.remote.corba._sk_IJSErrorReporter._execute(this, method.method_id, input, output); 
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
  public static boolean _execute(com.netscape.jsdebugging.remote.corba.IJSErrorReporter _self, int _method_id, org.omg.CORBA.portable.InputStream _input, org.omg.CORBA.portable.OutputStream _output) {
    switch(_method_id) {
    case 0: {
      java.lang.String arg0;
      arg0 = _input.read_string();
      java.lang.String arg1;
      arg1 = _input.read_string();
      int arg2;
      arg2 = _input.read_long();
      java.lang.String arg3;
      arg3 = _input.read_string();
      int arg4;
      arg4 = _input.read_long();
      int _result = _self.reportError(arg0,arg1,arg2,arg3,arg4);
      _output.write_long(_result);
      return false;
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
}
