package com.netscape.jsdebugging.remote.corba;
abstract public class _sk_ISourceTextProvider extends org.omg.CORBA.portable.Skeleton implements com.netscape.jsdebugging.remote.corba.ISourceTextProvider {
  protected _sk_ISourceTextProvider(java.lang.String name) {
    super(name);
  }
  protected _sk_ISourceTextProvider() {
    super();
  }
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:ISourceTextProvider:1.0"
  };
  public org.omg.CORBA.portable.MethodPointer[] _methods() {
    org.omg.CORBA.portable.MethodPointer[] methods = {
      new org.omg.CORBA.portable.MethodPointer("getAllPages", 0, 0),
      new org.omg.CORBA.portable.MethodPointer("refreshAllPages", 0, 1),
      new org.omg.CORBA.portable.MethodPointer("hasPage", 0, 2),
      new org.omg.CORBA.portable.MethodPointer("loadPage", 0, 3),
      new org.omg.CORBA.portable.MethodPointer("refreshPage", 0, 4),
      new org.omg.CORBA.portable.MethodPointer("getPageText", 0, 5),
      new org.omg.CORBA.portable.MethodPointer("getPageStatus", 0, 6),
      new org.omg.CORBA.portable.MethodPointer("getPageAlterCount", 0, 7),
    };
    return methods;
  }
  public boolean _execute(org.omg.CORBA.portable.MethodPointer method, org.omg.CORBA.portable.InputStream input, org.omg.CORBA.portable.OutputStream output) {
    switch(method.interface_id) {
    case 0: {
      return com.netscape.jsdebugging.remote.corba._sk_ISourceTextProvider._execute(this, method.method_id, input, output); 
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
  public static boolean _execute(com.netscape.jsdebugging.remote.corba.ISourceTextProvider _self, int _method_id, org.omg.CORBA.portable.InputStream _input, org.omg.CORBA.portable.OutputStream _output) {
    switch(_method_id) {
    case 0: {
      java.lang.String[] _result = _self.getAllPages();
      com.netscape.jsdebugging.remote.corba.ISourceTextProviderPackage.sequence_of_stringHelper.write(_output, _result);
      return false;
    }
    case 1: {
      _self.refreshAllPages();
      return false;
    }
    case 2: {
      java.lang.String arg0;
      arg0 = _input.read_string();
      boolean _result = _self.hasPage(arg0);
      _output.write_boolean(_result);
      return false;
    }
    case 3: {
      java.lang.String arg0;
      arg0 = _input.read_string();
      boolean _result = _self.loadPage(arg0);
      _output.write_boolean(_result);
      return false;
    }
    case 4: {
      java.lang.String arg0;
      arg0 = _input.read_string();
      _self.refreshPage(arg0);
      return false;
    }
    case 5: {
      java.lang.String arg0;
      arg0 = _input.read_string();
      java.lang.String _result = _self.getPageText(arg0);
      _output.write_string(_result);
      return false;
    }
    case 6: {
      java.lang.String arg0;
      arg0 = _input.read_string();
      int _result = _self.getPageStatus(arg0);
      _output.write_long(_result);
      return false;
    }
    case 7: {
      java.lang.String arg0;
      arg0 = _input.read_string();
      int _result = _self.getPageAlterCount(arg0);
      _output.write_long(_result);
      return false;
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
}
