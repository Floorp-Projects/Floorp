function Startup()
{
  const NC_NS = "http://home.netscape.com/NC-rdf#";
  const rdfSvcContractID = "@mozilla.org/rdf/rdf-service;1";
  const rdfSvcIID = Components.interfaces.nsIRDFService;
  var rdfService = Components.classes[rdfSvcContractID].getService(rdfSvcIID);

  const dlmgrContractID = "@mozilla.org/download-manager;1";
  const dlmgrIID = Components.interfaces.nsIDownloadManager;
  var downloadMgr = Components.classes[dlmgrContractID].getService(dlmgrIID);
  var ds = downloadMgr.datasource;
  
  const dateTimeContractID = "@mozilla.org/intl/scriptabledateformat;1";
  const dateTimeIID = Components.interfaces.nsIScriptableDateFormat;
  var dateTimeService = Components.classes[dateTimeContractID].getService(dateTimeIID);  

  var resource = rdfService.GetResource(window.arguments[0]);
  var dateStartedRes = rdfService.GetResource(NC_NS + "DateStarted");
  var dateEndedRes = rdfService.GetResource(NC_NS + "DateEnded");

  var dateStartedField = document.getElementById("dateStarted");
  var dateEndedField = document.getElementById("dateEnded");
  
  var dateStarted = ds.GetTarget(resource, dateStartedRes, true).QueryInterface(Components.interfaces.nsIRDFDate).Value;
  var dateEnded = ds.GetTarget(resource, dateStartedRes, true).QueryInterface(Components.interfaces.nsIRDFDate).Value;
  dateStarted = new Date(dateStarted/1000);
  dateStarted = dateTimeService.FormatDateTime("", dateTimeService.dateFormatShort, dateTimeService.timeFormatSeconds, dateStarted.getFullYear(), dateStarted.getMonth()+1, dateStarted.getDate(), dateStarted.getHours(), dateStarted.getMinutes(), dateStarted.getSeconds());
  dateEnded = new Date(dateEnded/1000);
  dateEnded = dateTimeService.FormatDateTime("", dateTimeService.dateFormatShort, dateTimeService.timeFormatSeconds, dateEnded.getFullYear(), dateEnded.getMonth()+1, dateEnded.getDate(), dateEnded.getHours(), dateEnded.getMinutes(), dateEnded.getSeconds());

  dateStartedField.setAttribute("value", dateStarted);
  dateEndedField.setAttribute("value", dateEnded);
}
  