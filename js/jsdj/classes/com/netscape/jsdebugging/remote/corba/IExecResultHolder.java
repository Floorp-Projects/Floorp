package com.netscape.jsdebugging.remote.corba;
final public class IExecResultHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IExecResult value;
  public IExecResultHolder() {
  }
  public IExecResultHolder(com.netscape.jsdebugging.remote.corba.IExecResult value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IExecResultHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IExecResultHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IExecResultHelper.type();
  }
}
