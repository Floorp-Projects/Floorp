package com.netscape.jsdebugging.remote.corba;
final public class IScriptSectionHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IScriptSection value;
  public IScriptSectionHolder() {
  }
  public IScriptSectionHolder(com.netscape.jsdebugging.remote.corba.IScriptSection value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IScriptSectionHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IScriptSectionHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IScriptSectionHelper.type();
  }
}
