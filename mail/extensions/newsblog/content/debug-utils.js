function enumerateInterfaces(obj)
{
  var interfaces = new Array();
  for (i in Components.interfaces)
  {
    try
    {
      obj.QueryInterface(Components.interfaces[i]);
      interfaces.push(i);
    }
    catch(e) {}
  }
  return interfaces;
}

function enumerateProperties(obj, excludeComplexTypes)
{
  var properties = "";
  for (p in obj)
  {
    try
    {
      if (excludeComplexTypes
          && (typeof obj[p] == 'object' || typeof obj[p] == 'function')) next;
      properties += p + " = " + obj[p] + "\n";
    }
    catch(e) {
      properties += p + " = " + e + "\n";
    }
  }
  return properties;
}

// minimal implementation of nsIOutputStream for use by dumpRDF, adapted from
// http://groups.google.com/groups?selm=20011203111618.C1302%40erde.jan.netgaroo.de
var DumpOutputStream = {
  write: function(buf, count) { dump(buf); return count; }
};

function dumpRDF( aDS ) {
  var serializer = Components.classes["@mozilla.org/rdf/xml-serializer;1"]
    .createInstance( Components.interfaces.nsIRDFXMLSerializer );
  
  serializer.init( aDS );
  
  serializer.QueryInterface( Components.interfaces.nsIRDFXMLSource )
    .Serialize( DumpOutputStream );
}
