package com.netscape.jsdebugging.remote.corba;
abstract public class _sk_IJSExecutionHook extends org.omg.CORBA.portable.Skeleton implements com.netscape.jsdebugging.remote.corba.IJSExecutionHook {
  protected _sk_IJSExecutionHook(java.lang.String name) {
    super(name);
  }
  protected _sk_IJSExecutionHook() {
    super();
  }
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:IJSExecutionHook:1.0"
  };
  public org.omg.CORBA.portable.MethodPointer[] _methods() {
    org.omg.CORBA.portable.MethodPointer[] methods = {
      new org.omg.CORBA.portable.MethodPointer("aboutToExecute", 0, 0),
    };
    return methods;
  }
  public boolean _execute(org.omg.CORBA.portable.MethodPointer method, org.omg.CORBA.portable.InputStream input, org.omg.CORBA.portable.OutputStream output) {
    switch(method.interface_id) {
    case 0: {
      return com.netscape.jsdebugging.remote.corba._sk_IJSExecutionHook._execute(this, method.method_id, input, output); 
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
  public static boolean _execute(com.netscape.jsdebugging.remote.corba.IJSExecutionHook _self, int _method_id, org.omg.CORBA.portable.InputStream _input, org.omg.CORBA.portable.OutputStream _output) {
    switch(_method_id) {
    case 0: {
      com.netscape.jsdebugging.remote.corba.IJSThreadState arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IJSThreadStateHelper.read(_input);
      com.netscape.jsdebugging.remote.corba.IJSPC arg1;
      arg1 = com.netscape.jsdebugging.remote.corba.IJSPCHelper.read(_input);
      _self.aboutToExecute(arg0,arg1);
      return false;
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
}
