/*
 * Hook used to trace events.
 */
function event_tracer (e)
{
    var name="", data="";

    switch (e.set)
    {
        case "server":
            name = e.destObject.connection.host;
            if (e.type == "rawdata")
                data = "'" + e.data + "'";
            break;

        case "channel":
            name = e.destObject.name;
            break;
            
        case "user":
            name = e.destObject.nick;
            break;

        case "httpdoc":
            name = e.destObject.server + e.destObject.path;
            if (e.destObject.state != "complete")
                data = "state: '" + e.destObject.state + "', recieved " +
                    e.destObject.data.length;
            else
                dd ("document done:\n" + dumpObjectTree (this));
            break;

        case "dcc-chat":
            name = e.destObject.host + ":" + e.destObject.port;
            if (e.type == "rawdata")
                data = "'" + e.data + "'";
            break;

        case "client":
            if (e.type == "do-connect")
                data = "attempt: " + e.attempt + "/" +
                    e.destObject.MAX_CONNECT_ATTEMPTS;
            break;

        default:
            break;
    }

    if (name)
        name = "[" + name + "]";

    str = "Level " + e.level + ": '" + e.type + "', " +
        e.set + name + "." + e.destMethod;
	if (data)
	  str += "\ndata   : " + data;

    dd (str);

    return true;

}
