package com.netscape.jsdebugging.remote.corba;
final public class TestInterfaceHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.TestInterface value;
  public TestInterfaceHolder() {
  }
  public TestInterfaceHolder(com.netscape.jsdebugging.remote.corba.TestInterface value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = TestInterfaceHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    TestInterfaceHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return TestInterfaceHelper.type();
  }
}
