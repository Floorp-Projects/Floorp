package com.netscape.jsdebugging.remote.corba;
final public class IScriptHookHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IScriptHook value;
  public IScriptHookHolder() {
  }
  public IScriptHookHolder(com.netscape.jsdebugging.remote.corba.IScriptHook value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IScriptHookHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IScriptHookHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IScriptHookHelper.type();
  }
}
