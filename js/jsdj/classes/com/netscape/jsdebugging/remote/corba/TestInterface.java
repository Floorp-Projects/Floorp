package com.netscape.jsdebugging.remote.corba;
public interface TestInterface extends org.omg.CORBA.Object {
  public java.lang.String getFirstAppInList();
  public void getAppNames(
    com.netscape.jsdebugging.remote.corba.StringReciever arg0
  );
  public com.netscape.jsdebugging.remote.corba.Thing[] getThings();
  public void callBounce(
    com.netscape.jsdebugging.remote.corba.StringReciever arg0,
    int arg1
  );
}
