package com.netscape.jsdebugging.remote.corba;
final public class IJSExecutionHookHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IJSExecutionHook value;
  public IJSExecutionHookHolder() {
  }
  public IJSExecutionHookHolder(com.netscape.jsdebugging.remote.corba.IJSExecutionHook value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IJSExecutionHookHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IJSExecutionHookHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IJSExecutionHookHelper.type();
  }
}
