package com.netscape.jsdebugging.remote.corba;
final public class IJSErrorReporterHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IJSErrorReporter value;
  public IJSErrorReporterHolder() {
  }
  public IJSErrorReporterHolder(com.netscape.jsdebugging.remote.corba.IJSErrorReporter value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IJSErrorReporterHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IJSErrorReporterHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IJSErrorReporterHelper.type();
  }
}
