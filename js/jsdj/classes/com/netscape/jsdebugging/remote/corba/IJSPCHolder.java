package com.netscape.jsdebugging.remote.corba;
final public class IJSPCHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IJSPC value;
  public IJSPCHolder() {
  }
  public IJSPCHolder(com.netscape.jsdebugging.remote.corba.IJSPC value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IJSPCHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IJSPCHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IJSPCHelper.type();
  }
}
