package com.netscape.jsdebugging.remote.corba;
final public class Thing {
  public java.lang.String s;
  public int i;
  public Thing() {
  }
  public Thing(
    java.lang.String s,
    int i
  ) {
    this.s = s;
    this.i = i;
  }
  public java.lang.String toString() {
    org.omg.CORBA.Any any = org.omg.CORBA.ORB.init().create_any();
    com.netscape.jsdebugging.remote.corba.ThingHelper.insert(any, this);
    return any.toString();
  }
}
