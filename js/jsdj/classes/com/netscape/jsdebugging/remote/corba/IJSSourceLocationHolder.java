package com.netscape.jsdebugging.remote.corba;
final public class IJSSourceLocationHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IJSSourceLocation value;
  public IJSSourceLocationHolder() {
  }
  public IJSSourceLocationHolder(com.netscape.jsdebugging.remote.corba.IJSSourceLocation value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IJSSourceLocationHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IJSSourceLocationHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IJSSourceLocationHelper.type();
  }
}
