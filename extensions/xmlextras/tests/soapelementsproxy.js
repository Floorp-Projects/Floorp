function Elements()
{
  this.PROXY = new SOAPProxy(this, 0,
    "http://213.23.125.181:8080/RPC", 
    "urn:SpheonJSOAPChemistry", 
    false);                  // false = not friendly / needs privileges
}

//  Object to proxy methods

Elements.prototype =
{
  PROXY: null,
  RECEIVER: function(proxy, headers, parameters)  //  This interprets all responses
  {
    proxy.PROXY.result = null;
    if (parameters != null) {
      var parameter = parameters[0];
      //  The following line can be used to force a result type
      //  parameter.schemaType = proxy.PROXY.getType("boolean");
      proxy.PROXY.result = parameter.value;
    }
    if (proxy.PROXY.oncompletion != null) {
      proxy.PROXY.oncompletion(proxy.PROXY.result);
    }
  },
  //  Proxy-specific items
  getElementBySymbol: function(symbol, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getElementBySymbol",
      null, new Array(new SOAPParameter(symbol,"symbol",null,type)),
      this.RECEIVER, oncompletion);
  },
  getNumberBySymbol: function(symbol, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getNumberBySymbol",
      null, new Array(new SOAPParameter(symbol,"symbol",null,type)),
      this.RECEIVER, oncompletion);
  },
  getNameBySymbol: function(symbol, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getNameBySymbol",
      null, new Array(new SOAPParameter(symbol,"symbol",null,type)),
      this.RECEIVER, oncompletion);
  },
  getMassBySymbol: function(symbol, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getMassBySymbol",
      null, new Array(new SOAPParameter(symbol,"symbol",null,type)),
      this.RECEIVER, oncompletion);
  },
  getMeltingPointBySymbol: function(symbol, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getMeltingPointBySymbol",
      null, new Array(new SOAPParameter(symbol,"symbol",null,type)),
      this.RECEIVER, oncompletion);
  },
  getBoilingPointBySymbol: function(symbol, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getBoilingBointBySymbol",
      null, new Array(new SOAPParameter(symbol,"symbol",null,type)),
      this.RECEIVER, oncompletion);
  },
  getFoundBySymbol: function(symbol, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "foundBySymbol",
      null, new Array(new SOAPParameter(symbol,"symbol",null,type)),
      this.RECEIVER, oncompletion);
  },
  getElementByNumber: function(number, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getElementByNumber",
      null, new Array(new SOAPParameter(number,"number",null,type)),
      this.RECEIVER, oncompletion);
  },
  getSymbolByNumber: function(number, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getSymbolByNumber",
      null, new Array(new SOAPParameter(number,"number",null,type)),
      this.RECEIVER, oncompletion);
  },
  getNameByNumber: function(number, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getNameByNumber",
      null, new Array(new SOAPParameter(number,"number",null,type)),
      this.RECEIVER, oncompletion);
  },
  getMassByNumber: function(number, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getMassByNumber",
      null, new Array(new SOAPParameter(number,"number",null,type)),
      this.RECEIVER, oncompletion);
  },
  getMeltingPointByNumber: function(number, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getMeltingPointByNumber",
      null, new Array(new SOAPParameter(number,"number",null,type)),
      this.RECEIVER, oncompletion);
  },
  getBoilingPointByNumber: function(number, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "getBoilingPointByNumber",
      null, new Array(new SOAPParameter(number,"number",null,type)),
      this.RECEIVER, oncompletion);
  },
  getFoundByNumber: function(number, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "foundByNumber",
      null, new Array(new SOAPParameter(number,"number",null,type)),
      this.RECEIVER, oncompletion);
  }
}
