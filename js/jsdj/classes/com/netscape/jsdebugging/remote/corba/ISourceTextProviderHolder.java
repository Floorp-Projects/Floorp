package com.netscape.jsdebugging.remote.corba;
final public class ISourceTextProviderHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.ISourceTextProvider value;
  public ISourceTextProviderHolder() {
  }
  public ISourceTextProviderHolder(com.netscape.jsdebugging.remote.corba.ISourceTextProvider value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = ISourceTextProviderHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    ISourceTextProviderHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return ISourceTextProviderHelper.type();
  }
}
