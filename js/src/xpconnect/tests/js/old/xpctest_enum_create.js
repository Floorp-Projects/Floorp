if (typeof document == "undefined") /* in xpcshell */
    dumpln = print;
else
    dumpln = function (str) {dump (str + "\n");}

function dumpObject (o, pfx, sep)
{
    var p;
    var s = "";

    sep = (typeof sep == "undefined") ? " = " : sep;
    pfx = (typeof pfx == "undefined") ? "" : pfx;
    

    for (p in o)
    {
        if (typeof (o[p]) != "function")
            s += pfx + p + sep + o[p] + "\n";
        else
            s += pfx + p + sep + "function\n";
    }

    return s;

}

function bruteForceEnumeration(ignore)
{
    var interfaceInfo = new Object();

loop: for (var c in Components.classes)
    {
        var cls = Components.classes[c];
        dumpln ("**");
        dumpln (" * ContractID: '" + cls.name + "'");
        dumpln (" * CLSID: " + cls.number);

        if(ignore) {
            for(var i = 0; i < ignore.length; i++) {
                if(0 == cls.name.indexOf(ignore[i])) {
                    dumpln (" * This one might cause a crash - SKIPPING");
                    continue loop;                
                }
            }
        }

        try
        {
            var ins = cls.createInstance();
            for (var i in Components.interfaces)
            {
                try
                {
                    qi = ins.QueryInterface (Components.interfaces[i]);
                    dumpln (" * Supports Interface: " + i);
                    if (typeof interfaceInfo[i] == "undefined")
                    {
                        interfaceInfo[i] = dumpObject (qi, " ** ");
                    }
                    dumpln (interfaceInfo[i]);
                }
                catch (e)
                {
                    /* nada */
                }
            }
        }
        catch (e)
        {
            dumpln (" * createInstance FAILED:");
            dumpln (dumpObject (e));
        }
        
        dumpln ("");
        
    }

    dumpln ("**");
    dumpln (" * Interface Information :");
    dumpln ("");
    // dump of giant string does not always work, split it up.
    var bulk = dumpObject (interfaceInfo, (void 0), "::\n")
    var lines = bulk.split("\n");
    for(var i = 0; i < lines.length; i++)
        dumpln (lines[i]);
    
}

/** tests **/
var totalClasses = 0,
        nonameClasses = 0,
        strangeClasses = 0,
        totalIfaces = 0,
        strangeIfaces = 0;

dumpln ("**> Enumerating Classes");
for (var c in Components.classes)
{
    totalClasses++;
    if (Components.classes[c].name == "")
    {
        dumpln ("CLSID " + c + " has no contractID.");
        nonameClasses++;
    }
    else
        if (c.search(/^component:\/\//) == -1)
        {
            dumpln ("Strange contractID '" + c + "'");
            strangeClasses++;
        }
}

dumpln ("**> Enumerating Interfaces");
for (var i in Components.interfaces)
{
    if (i != "QueryInterface")
    {
        totalIfaces++;
    
        if (i.search(/^nsI/) == -1)
        {
            dumpln ("Strange interface name '" + i + "'");
            strangeIfaces++;
        }
    }
}

dumpln ("** Enumerated " + totalClasses + " classes");
dumpln (" * " + nonameClasses + " without contractIDs");
dumpln (" * " + strangeClasses + " with strange names");
dumpln ("");
dumpln ("** Enumerated " + totalIfaces + " interfaces");
dumpln (" * " + strangeIfaces + " with strange names");

var contractIDsTo_NOT_Create = [

// fixed    "@mozilla.org/prefwindow;1", // nsPrefWindow::~nsPrefWindow() releases null service
// fixed /* BUG 11511 */    "@mozilla.org/rdf/datasource;1?name=addresscard",      // nsAbRDFDataSource::~nsAbRDFDataSource calls mRDFService->UnregisterDataSource(this); even though it was not registered
// fixed /* BUG 11511 */    "@mozilla.org/rdf/datasource;1?name=addressdirectory", // nsAbRDFDataSource::~nsAbRDFDataSource calls mRDFService->UnregisterDataSource(this); even though it was not registered
// fixed    "@mozilla.org/addressbook/directoryproperty;1",  // fails to init refcount

// fixed    "@mozilla.org/rdf/datasource;1?name=local-store", //NS_NewLocalStore calls stuff that asserts
// fixed    "@mozilla.org/rdf/datasource;1?name=xpinstall-update-notifier", //RDFXMLDataSourceImpl::Refresh does CreateInstance of parser but fails to check the result (I think I've seen this elsewhere)
// fixed    "@mozilla.org/rdf/xul-template-builder;1",    //RDFXMLDataSourceImpl::Refresh does CreateInstance of parser but fails to check the result (I think I've seen this elsewhere)

// fixed    "@mozilla.org/rdf/xul-content-sink;1", // nsXULContentSink.cpp, XULContentSinkImpl::XULContentSinkImpl needs to init mNameSpaceManager

/* dp claims to have fixes coming */   "@mozilla.org/image/decoder;1?type=image/",  // PNGDecoder::QueryInterface and JPGDecoder::QueryInterface, do NS_INIT_ISUPPORTS() in QueryInterface! (npunn)

/* BUG 11507 */    "@mozilla.org/wallet;1", // WalletFactoryImpl::CreateInstance broken : calls "delete inst" then calls "NS_IF_RELEASE(inst)"

/* BUG 11509 */    "@mozilla.org/messengercompose/compose;1",              // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)
/* BUG 11509 */    "@mozilla.org/messenger;1",                             // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)
/* BUG 11509 */    "@mozilla.org/rdf/datasource;1?name=msgaccountmanager", // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service) 
/* BUG 11509 */    "@mozilla.org/rdf/datasource;1?name=mailnewsfolders",   // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)
/* BUG 11509 */    "@mozilla.org/rdf/datasource;1?name=msgnotifications",  // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)
/* BUG 11509 */    "@mozilla.org/rdf/datasource;1?name=mailnewsmessages",  // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)



/* BUG 11510 */    "@mozilla.org/rdf/xul-key-listener;1",   // RDFFactoryImpl::CreateInstance asserts when creating if requested interface is nsISupports!
/* BUG 11510 */    "@mozilla.org/rdf/xul-popup-listener;1", // RDFFactoryImpl::CreateInstance asserts when creating if requested interface is nsISupports!
/* BUG 11510 */    "@mozilla.org/rdf/xul-focus-tracker;1",  // RDFFactoryImpl::CreateInstance asserts when creating if requested interface is nsISupports!


/* BUG 11512 */    "@mozilla.org/rdf/datasource;1?name=files", // FileSystemDataSource::~FileSystemDataSource calls gRDFService->UnregisterDataSource(this); even though it was not registered
/* BUG 11512 */    "@mozilla.org/rdf/datasource;1?name=find", // FindDataSource::~FindDataSource calls gRDFService->UnregisterDataSource(this) even if it was not actaully registered.

/* BUG 11514 */    "@mozilla.org/rdf/datasource;1?name=msgaccounts", // nsMsgAccountDataSource::QueryInterface is COMPLETELY <censor>screwed</censor> up

/* BUG 11516 */    "@mozilla.org/rdf/xul-sort-service;1",  // XULSortServiceImpl releases itself in its own destructor!

/* BUG 11570 */    "componment://netscape/intl/charsetconvertermanager", // another case where CreateInstance returned NS_OK, but the instance pointer was null!

/* BUG 11571 */    "@mozilla.org/rdf/datasource;1?name=mail-messageview", // nsMessageViewDataSource::RemoveDataSource uses mDataSource without checking for validity


/* BUG 11575 */    "@mozilla.org/rdf/resource-factory;1",  // calling a property - trying to copy a null value in nsRDFResource::GetValue


/* BUG 11579 */   "@mozilla.org/messenger/maildb;1", // calling a property - nsMsgDatabase::m_newSet used but not set

/* BUG 11580 */    "@mozilla.org/messenger/identity;1",  // calling a property - nsMsgIdentity::getCharPref  is null
/* BUG 11580 */    "@mozilla.org/messenger/server;1?type=", // calling a property - nsMsgIncomingServer::getCharPref uses m_prefs which is null

/* ASSERTION OK? */    "@mozilla.org/messenger/account;1",   // calling a property - nsMsgAccount::GetIncomingServer asserts because m_accountKey is null

];


dumpln ("-------------------------------------------------------");
dumpln (" Now let's create every component we can find...");

if(contractIDsTo_NOT_Create.length) {
    dumpln ("...except the following 'cuz they've been know to cause CRASHES!...")
    for(var i = 0; i < contractIDsTo_NOT_Create.length; i++)
        dumpln ("  "+contractIDsTo_NOT_Create[i]);
    dumpln ();
}
bruteForceEnumeration(contractIDsTo_NOT_Create);
