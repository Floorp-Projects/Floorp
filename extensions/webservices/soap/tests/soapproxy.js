function SOAPProxy(proxy, soapVersion, transportURI, target, verifySourceHeader)
{
  this.proxy = proxy;
  this.call = new SOAPCall();
  this.call.transportURI = transportURI;
  this.call.verifySourceHeader = verifySourceHeader;
  this.target = target;
  this.soapVersion = soapVersion;
  var encoding = new SOAPEncoding();
  if (soapVersion == 0) {
    encoding = encoding.getAssociatedEncoding("http://schemas.xmlsoap.org/soap/encoding/",false);
  }
  else {
    encoding = encoding.getAssociatedEncoding("http://www.w3.org/2001/09/soap-encoding",false);
  }
  this.call.encoding = encoding; //  Get the encoding first in case we need it
}

SOAPProxy.prototype =
{
  proxy: null,       //  Specific proxy
  active: null,      //  Asynchronous call which is currently active, if any
  call: null,        //  Call object which receives encoding
  target: null,      //  Target object for encoding
  soapVersion: null, //  SOAP version for encoding
  method: null,      //  Method, if any, for decoding of results
  complete: null,    //  Handler to receive results, if any
  result: null,      //  Result which may be returned from synchronous call
  oncompletion: null,//  Higher-level result handler
  invoke: function(method, headers, parameters, complete, oncompletion)
  {
    if (this.active != null)
      this.active.abort();

    this.method = method;
    this.complete = complete;
    this.result = null;
    this.oncompletion = oncompletion;
    this.call.encode(
      this.soapVersion,
      method,
      this.target,
      headers != null ? headers.length : 0, headers, 
      parameters != null ? parameters.length : 0, parameters);

    if (!this.call.verifySourceHeader)
      netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");

    if (oncompletion != null)
    {
      this.active = this.call.asyncInvoke(this);
      return;
    }
    else
    {
      this.active = null;
      var response = null;
      var status = 0;
      try {
        response = this.call.invoke();
      }
      catch (e) {
        status = e;
      }
      this.handleResponse(response, this.call, status, true);
      return this.result;
    }
  },
  handleResponse: function(response, call, status, complete)
  {
    var headers = null;
    var parameters = null;
    if (status != 0) {
      var report = "SOAP {" + this.target + "}" + this.method + " call failed to call server " 
        + this.call.transportURI +": " + status;
      this.log(status, report);
    }
    else {
      // Was there a SOAP fault in the response?
      var fault = response.fault;
      if (fault != null) {
        var report = "SOAP {" + this.target + "}" + this.method + " call resulted in fault: {" 
          + fault.faultNamespaceURI + "}" + fault.faultCode + ": " + fault.faultString;
        status = 0x80004005;
        this.log(status, report);
      }
      else if (response != null) {
        headers = response.getHeaderBlocks({});
        parameters = response.getParameters(this.method == null,{});
      }
    }
    this.complete(this.proxy,headers,parameters);
    return true;
  },
  getType: function(name, uri)
  {
    if (uri == null) {
      uri = "http://www.w3.org/2001/XMLSchema";
    }
    try {
      var result = this.call.encoding.schemaCollection.getType(name, uri);
      if (result != null) return result;
    }
    catch (e) {}
    var report = "Type {" + uri + "}" + name + " not found.";
    throw this.log(0x80004005, report);
  },
  loadTypes: function(uri)
  {
    try {
      this.call.collection.load(uri);
      return;
    }
    catch (e) {}
    var report = "Unable to load schema file " + uri;
    throw this.log(0x80004005, report);
  },
  log: function(status, message)
  {
    alert(message);  //  Comment this out to avoid raising descriptive dialog boxes for failed SOAP calls
    return status;
  }
}
