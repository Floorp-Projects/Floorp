package com.netscape.jsdebugging.remote.corba;
public class _st_ISourceTextProvider extends org.omg.CORBA.portable.ObjectImpl implements com.netscape.jsdebugging.remote.corba.ISourceTextProvider {
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:ISourceTextProvider:1.0"
  };
  public java.lang.String[] getAllPages() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getAllPages", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      java.lang.String[] _result;
      _result = com.netscape.jsdebugging.remote.corba.ISourceTextProviderPackage.sequence_of_stringHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getAllPages();
    }
  }
  public void refreshAllPages() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("refreshAllPages", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      refreshAllPages();
    }
  }
  public boolean hasPage(
    java.lang.String arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("hasPage", true);
      _output.write_string(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      boolean _result;
      _result = _input.read_boolean();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return hasPage(
        arg0
      );
    }
  }
  public boolean loadPage(
    java.lang.String arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("loadPage", true);
      _output.write_string(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      boolean _result;
      _result = _input.read_boolean();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return loadPage(
        arg0
      );
    }
  }
  public void refreshPage(
    java.lang.String arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("refreshPage", true);
      _output.write_string(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      refreshPage(
        arg0
      );
    }
  }
  public java.lang.String getPageText(
    java.lang.String arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getPageText", true);
      _output.write_string(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      java.lang.String _result;
      _result = _input.read_string();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getPageText(
        arg0
      );
    }
  }
  public int getPageStatus(
    java.lang.String arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getPageStatus", true);
      _output.write_string(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      int _result;
      _result = _input.read_long();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getPageStatus(
        arg0
      );
    }
  }
  public int getPageAlterCount(
    java.lang.String arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getPageAlterCount", true);
      _output.write_string(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      int _result;
      _result = _input.read_long();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getPageAlterCount(
        arg0
      );
    }
  }
}
