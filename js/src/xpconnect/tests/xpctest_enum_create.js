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
        dumpln (" * ProgID: '" + cls.name + "'");
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
        dumpln ("CLSID " + c + " has no progID.");
        nonameClasses++;
    }
    else
        if (c.search(/^component:\/\//) == -1)
        {
            dumpln ("Strange progID '" + c + "'");
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
dumpln (" * " + nonameClasses + " without progIDs");
dumpln (" * " + strangeClasses + " with strange names");
dumpln ("");
dumpln ("** Enumerated " + totalIfaces + " interfaces");
dumpln (" * " + strangeIfaces + " with strange names");

var progIDsTo_NOT_Create = [

// fixed    "component://netscape/prefwindow", // nsPrefWindow::~nsPrefWindow() releases null service
// fixed /* BUG 11511 */    "component://netscape/rdf/datasource?name=addresscard",      // nsAbRDFDataSource::~nsAbRDFDataSource calls mRDFService->UnregisterDataSource(this); even though it was not registered
// fixed /* BUG 11511 */    "component://netscape/rdf/datasource?name=addressdirectory", // nsAbRDFDataSource::~nsAbRDFDataSource calls mRDFService->UnregisterDataSource(this); even though it was not registered
// fixed    "component://netscape/addressbook/directoryproperty",  // fails to init refcount

// fixed    "component://netscape/rdf/datasource?name=local-store", //NS_NewLocalStore calls stuff that asserts
// fixed    "component://netscape/rdf/datasource?name=xpinstall-update-notifier", //RDFXMLDataSourceImpl::Refresh does CreateInstance of parser but fails to check the result (I think I've seen this elsewhere)
// fixed    "component://netscape/rdf/xul-template-builder",    //RDFXMLDataSourceImpl::Refresh does CreateInstance of parser but fails to check the result (I think I've seen this elsewhere)


/* dp claims to have fixes coming */   "component://netscape/image/decoder&type=image/",  // PNGDecoder::QueryInterface and JPGDecoder::QueryInterface, do NS_INIT_REFCNT() in QueryInterface! (npunn)
/* works now ??? */    "component://netscape/messenger/maildb", // calling a property - nsMsgDatabase::m_newSet used but not set

/* BUG 11507 */    "component://netscape/wallet", // WalletFactoryImpl::CreateInstance broken : calls "delete inst" then calls "NS_IF_RELEASE(inst)"

/* BUG 11509 */    "component://netscape/messengercompose/compose",              // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)
/* BUG 11509 */    "component://netscape/messenger",                             // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)
/* BUG 11509 */    "component://netscape/rdf/datasource?name=msgaccountmanager", // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service) 
/* BUG 11509 */    "component://netscape/rdf/datasource?name=mailnewsfolders",   // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)
/* BUG 11509 */    "component://netscape/rdf/datasource?name=msgnotifications",  // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)
/* BUG 11509 */    "component://netscape/rdf/datasource?name=mailnewsmessages",  // ASSERTION in nsMsgAccountManager::prefService() (needs prefs service)



/* BUG 11510 */    "component://netscape/rdf/xul-key-listener",   // RDFFactoryImpl::CreateInstance asserts when creating if requested interface is nsISupports!
/* BUG 11510 */    "component://netscape/rdf/xul-popup-listener", // RDFFactoryImpl::CreateInstance asserts when creating if requested interface is nsISupports!
/* BUG 11510 */    "component://netscape/rdf/xul-focus-tracker",  // RDFFactoryImpl::CreateInstance asserts when creating if requested interface is nsISupports!


/* BUG 11512 */    "component://netscape/rdf/datasource?name=files", // FileSystemDataSource::~FileSystemDataSource calls gRDFService->UnregisterDataSource(this); even though it was not registered


/* BUG 11514 */    "component://netscape/rdf/datasource?name=msgaccounts", // nsMsgAccountDataSource::QueryInterface is COMPLETELY <censor>screwed</censor> up

/* BUG 11516 */    "component://netscape/rdf/xul-sort-service",  // XULSortServiceImpl releases itself in its own destructor!

// this bug filing business is hard work! I'll file the rest later...

    "component://netscape/rdf/resource-factory",  // calling a property - trying to copy a null value in nsRDFResource::GetValue
    "component://netscape/messenger/identity",  // calling a property - nsMsgIdentity::getCharPref  is null
    "component://netscape/messenger/account",   // calling a property - nsMsgAccount::GetIncomingServer asserts because m_accountKey is null
    "component://netscape/messenger/server&type=", // calling a property - nsMsgIncomingServer::getCharPref uses m_prefs which is null
    "componment://netscape/intl/charsetconvertermanager", // another case where CreateInstance returned NS_OK, but the instance pointer was null!
    "component://netscape/rdf/datasource?name=mail-messageview", // nsMessageViewDataSource::RemoveDataSource uses mDataSource without checking for validity


    "component://netscape/rdf/xul-content-sink", // nsXULContentSink.cpp, XULContentSinkImpl::XULContentSinkImpl needs to init mNameSpaceManager
    "component://netscape/rdf/datasource?name=find", // FindDataSource::~FindDataSource calls gRDFService->UnregisterDataSource(this) even if it was not actaully registered.



];


dumpln ("-------------------------------------------------------");
dumpln (" Now let's create every component we can find...");

if(progIDsTo_NOT_Create.length) {
    dumpln ("...except the following 'cuz they've been know to cause CRASHES!...")
    for(var i = 0; i < progIDsTo_NOT_Create.length; i++)
        dumpln ("  "+progIDsTo_NOT_Create[i]);
    dumpln ();
}
bruteForceEnumeration(progIDsTo_NOT_Create);
