package com.netscape.jsdebugging.remote.corba;
abstract public class _sk_TestInterface extends org.omg.CORBA.portable.Skeleton implements com.netscape.jsdebugging.remote.corba.TestInterface {
  protected _sk_TestInterface(java.lang.String name) {
    super(name);
  }
  protected _sk_TestInterface() {
    super();
  }
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:TestInterface:1.0"
  };
  public org.omg.CORBA.portable.MethodPointer[] _methods() {
    org.omg.CORBA.portable.MethodPointer[] methods = {
      new org.omg.CORBA.portable.MethodPointer("getFirstAppInList", 0, 0),
      new org.omg.CORBA.portable.MethodPointer("getAppNames", 0, 1),
      new org.omg.CORBA.portable.MethodPointer("getThings", 0, 2),
      new org.omg.CORBA.portable.MethodPointer("callBounce", 0, 3),
    };
    return methods;
  }
  public boolean _execute(org.omg.CORBA.portable.MethodPointer method, org.omg.CORBA.portable.InputStream input, org.omg.CORBA.portable.OutputStream output) {
    switch(method.interface_id) {
    case 0: {
      return com.netscape.jsdebugging.remote.corba._sk_TestInterface._execute(this, method.method_id, input, output); 
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
  public static boolean _execute(com.netscape.jsdebugging.remote.corba.TestInterface _self, int _method_id, org.omg.CORBA.portable.InputStream _input, org.omg.CORBA.portable.OutputStream _output) {
    switch(_method_id) {
    case 0: {
      java.lang.String _result = _self.getFirstAppInList();
      _output.write_string(_result);
      return false;
    }
    case 1: {
      com.netscape.jsdebugging.remote.corba.StringReciever arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.StringRecieverHelper.read(_input);
      _self.getAppNames(arg0);
      return false;
    }
    case 2: {
      com.netscape.jsdebugging.remote.corba.Thing[] _result = _self.getThings();
      com.netscape.jsdebugging.remote.corba.TestInterfacePackage.sequence_of_ThingHelper.write(_output, _result);
      return false;
    }
    case 3: {
      com.netscape.jsdebugging.remote.corba.StringReciever arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.StringRecieverHelper.read(_input);
      int arg1;
      arg1 = _input.read_long();
      _self.callBounce(arg0,arg1);
      return false;
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
}
