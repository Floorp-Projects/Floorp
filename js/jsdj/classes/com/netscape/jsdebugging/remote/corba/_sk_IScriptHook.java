package com.netscape.jsdebugging.remote.corba;
abstract public class _sk_IScriptHook extends org.omg.CORBA.portable.Skeleton implements com.netscape.jsdebugging.remote.corba.IScriptHook {
  protected _sk_IScriptHook(java.lang.String name) {
    super(name);
  }
  protected _sk_IScriptHook() {
    super();
  }
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:IScriptHook:1.0"
  };
  public org.omg.CORBA.portable.MethodPointer[] _methods() {
    org.omg.CORBA.portable.MethodPointer[] methods = {
      new org.omg.CORBA.portable.MethodPointer("justLoadedScript", 0, 0),
      new org.omg.CORBA.portable.MethodPointer("aboutToUnloadScript", 0, 1),
    };
    return methods;
  }
  public boolean _execute(org.omg.CORBA.portable.MethodPointer method, org.omg.CORBA.portable.InputStream input, org.omg.CORBA.portable.OutputStream output) {
    switch(method.interface_id) {
    case 0: {
      return com.netscape.jsdebugging.remote.corba._sk_IScriptHook._execute(this, method.method_id, input, output); 
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
  public static boolean _execute(com.netscape.jsdebugging.remote.corba.IScriptHook _self, int _method_id, org.omg.CORBA.portable.InputStream _input, org.omg.CORBA.portable.OutputStream _output) {
    switch(_method_id) {
    case 0: {
      com.netscape.jsdebugging.remote.corba.IScript arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IScriptHelper.read(_input);
      _self.justLoadedScript(arg0);
      return false;
    }
    case 1: {
      com.netscape.jsdebugging.remote.corba.IScript arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IScriptHelper.read(_input);
      _self.aboutToUnloadScript(arg0);
      return false;
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
}
