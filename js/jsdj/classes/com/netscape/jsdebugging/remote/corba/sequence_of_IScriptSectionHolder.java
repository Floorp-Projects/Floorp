package com.netscape.jsdebugging.remote.corba;
final public class sequence_of_IScriptSectionHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IScriptSection[] value;
  public sequence_of_IScriptSectionHolder() {
  }
  public sequence_of_IScriptSectionHolder(com.netscape.jsdebugging.remote.corba.IScriptSection[] value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = sequence_of_IScriptSectionHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    sequence_of_IScriptSectionHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return sequence_of_IScriptSectionHelper.type();
  }
}
