package com.netscape.jsdebugging.remote.corba;
final public class IScriptHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IScript value;
  public IScriptHolder() {
  }
  public IScriptHolder(com.netscape.jsdebugging.remote.corba.IScript value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IScriptHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IScriptHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IScriptHelper.type();
  }
}
