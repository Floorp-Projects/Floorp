
function onLoad()
{

    initHost(client);
    initStatic();
    mainStep();
    
}

function onUnload()
{

    client.quit ("re-load");
    
}

/* toolbaritem click */
function onTBIClick (id)
{
    var tbi = document.getElementById (id);
    var view = client.viewsArray[tbi.getAttribute("viewKey")];

    setCurrentObject (view);
    
}

function onInputKeyUp (e)
{
    
    switch (e.which)
    {        
        case 13: /* CR */
            e.line = e.target.value;
            onInputCompleteLine (e);
            break;

        case 38: /* up */
            break;

        case 40: /* down */
            break;
            
    }

}

function onInputCompleteLine(e)
{

    if (e.target.getAttribute ("expanded") != "YES")
    {
        if (e.line[0] == client.COMMAND_CHAR)
        {
            var ary = e.line.substr(1, e.line.length).match (/(\S+)? ?(.*)/);
            var command = ary[1];
            
            if (command[0].search (/[\[\{\(]/) == 0) /* request to expand */
            {
                e.target.setAttribute("expanded", "YES");
                switch (command[0])
                {
                    case "[":
                        e.target.setAttribute("collapseChar", "]");
                        break;

                    case "{":
                        e.target.setAttribute("collapseChar", "}");
                        break;
                        
                    case "(":
                        e.target.setAttribute("collapseChar", ")");
                        break;        
                }
                e.target.style.height = client.EXPAND_HEIGHT;
            }
            else /* normal command */
            {
                var destMethod = "onInput" + command[0].toUpperCase() +
                    command.substr (1, command.length).toLowerCase();

                if (typeof client[destMethod] != "function")
                {
                    client.display ("Unknown command '" + command + "'.",
                                    "ERROR");
                    return false;
                }
                
                var type = "input-" + command.toLowerCase();
                var ev = new CEvent ("client", type, client, destMethod);

                ev.command = command;
                ev.inputData =  ary[2] ? ary[2] : "";

                ev.target = client.currentObject;

                switch (client.currentObject.TYPE) /* set up event props */
                {
                    case "IRCChannel":
                        ev.channel = ev.target;
                        ev.server = ev.channel.parent;
                        ev.network = ev.server.parent;
                        break;

                    case "IRCUser":
                        ev.user = ev.target;
                        ev.server = ev.user.parent;
                        ev.network = ev.server.parent;
                        break;

                    case "IRCChanUser":
                        ev.user = ev.target;
                        ev.channel = ev.user.parent;
                        ev.server = ev.channel.parent;
                        ev.network = ev.server.parent;
                        break;

                    case "IRCNetwork":
                        ev.network = client.currentObject;
                        if (ev.network.isConnected())
                            ev.server = ev.network.primServ;
                        break;

                    case "IRCClient":
                        if (client.lastNetwork)
                        {
                            ev.network = client.lastNetwork;
                            if (ev.network.isConnected())
                                ev.server = ev.network.primServ;
                        }
                        break;

                    default:
                        /* no setup for unknown object */
                        
                }

                client.eventPump.addEvent (ev);
            }
        }
        else /* plain text */
        {
            client.sayToCurrentTarget (e.target.value);
            e.target.value = "";            
        }
    }
    else /* input-box is expanded */
    {
        var lines = e.target.value.split("\n");
        for (i in lines)
            if (lines[i] == "")
                arrayRemoveAt (lines, i);
        var lastLine = lines[lines.length - 1];

        dd ("lines: " + lines);
        
        if (lastLine.replace(/s*$/,"") ==
            e.target.getAttribute ("collapseChar"))
        {
            dd ("collapsing...");
            
            e.target.setAttribute("expanded", "NO");
            e.target.style.height = client.COLLAPSE_HEIGHT;
            e.target.value = "";
            for (var i = 1; i < lines.length - 1; i++)
                client.sayToCurrentTarget (lines[i]);
        }
    }
    
}

client.onInputNetwork =
function clie_inetwork (e)
{
    if (!e.inputData)
    {
        client.display ("network <network-name>", "USAGE");
        return false;
    }

    var net = client.networks[e.inputData];
    
    if (net)
        client.lastNetwork = net;
    else
        client.display ("Unknown network '" + e.inputData + "'", "ERROR");

    return true;
    
}

client.onInputAttach =
function cli_iattach (e)
{
    var net;
    
    if (!e.inputData)
    {
        if (client.lastNetwork)
        {        
            client.display ("No network specified network, " +
                            "Using '" + client.lastNetwork.name + "'",
                            "NOTICE");
            net = client.lastNetwork;
        }
        else
        {
            client.display ("No network specified, and no default network " +
                            "is in place.", "ERROR");
            client.display ("attach <network-name>.", "USAGE");
            return false;
        }
    }
    else
    {
        net = client.networks[e.inputData];
        if (!net)
            client.display ("Unknown network '" + e.inputData + "'", "ERROR");
        client.lastNetwork = net;
    }

    net.connect();
    net.display ("Connecting...", "INFO");
    setCurrentObject(net);
    
}
    
client.onInputMe =
function cli_ime (e)
{
    if (e.channel)
    {
        e.channel.act (e.inputData);
        client.channel.display (e.inputData, "ACTION", "!ME");
    }
}

client.onInputNick =
function cli_inick (e)
{

    if (!e.inputData)
        return false;
    
    if (e.server) 
        e.server.sendData ('NICK ' + e.inputData + '\n');
    else
        CIRCNetwork.prototype.INITIAL_NICK = e.inputData;
    
}

client.onInputName =
function cli_iname (e)
{

    if (!e.inputData)
        return false;
    
    CIRCNetwork.prototype.INITIAL_NAME = e.inputData;
    
}

client.onInputDesc =
function cli_idesc (e)
{

    if (!e.inputData)
        return false;
    
    CIRCNetwork.prototype.INITIAL_DESC = e.inputData;
    
}

client.onInputRaw =
function cli_iraw (e)
{

    client.primNet.primServ.sendData (e.inputData + "\n");
    
}

client.onInputEval =
function cli_ieval (e)
{

    try
    {
        rv = String(eval (e.inputData));
        if (rv.indexOf ("\n") == -1)
            client.display ("(" + e.inputData + ") " + rv, "EVAL");
        else
            client.display ("(" + e.inputData + ")\n" + rv, "EVAL");
    }
    catch (ex)
    {
        client.display (String(ex), "ERROR");
    }
    
}
    
client.onInputJ = client.onInputJoin =
function cli_ijoin (e)
{
    if (!e.network || !e.network.isConnected())
    {
        if (!e.network)
            client.display ("No network selected.", "ERROR");
        else
            client.display ("Network '" + e.network.name + " is not connected.",
                            "ERROR");        
        return false;
    }
    
    var name = e.inputData.match(/\S+/);
    if (!name)
    {
        client.display ("join [#|&]<channel-name>", "USAGE");
        return false;
    }

    if ((name[0] != "#") && (name[0] != "&"))
        name = "#" + name;

    e.channel = e.server.addChannel (String(name));
    e.channel.join();
    e.channel.display ("Joining...", "INFO");
    setCurrentObject(e.channel);
    
}

client.onInputP = client.onInputPart = client.onInputLeave =
function cli_ipart (e)
{
    if (!e.channel)
        return false;

    e.channel.part();    
    
}

CIRCNetwork.prototype.onNotice =
function my_notice (e)
{

    this.display (e.meat, "NOTICE");
    
}

CIRCChannel.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    
    e.user.display (e.meat, "PRIVMSG");
    
    if (e.meat.indexOf (client.prefix) == 0)
    {
        try
        {
            var v = eval(e.meat.substring (client.prefix.length,
                                           e.meat.length));
        }
        catch (ex)
        {
            this.say (e.user.nick + ": " + String(ex));
            return false;
        }
        
        if (typeof (v) != "undefined")
        {						
            if (v != null)                
                v = String(v);
            else
                v = "null";
            
            var rsp = e.user.nick + ", your result is,";
            
            if (v.indexOf ("\n") != -1)
                rsp += "\n";
            else
                rsp += " ";
            
            this.say (rsp + v);
        }
    }

    return true;
    
}

/* end of names */
CIRCChannel.prototype.on366 =
function my_366 (e)
{

    if (!this.list)
        this.list = new CListBox();
    else
        this.list.clear();

    for (var u in this.users)
        this.list.add (this.users[u].getDecoratedNick());
    
}    
    
CIRCChannel.prototype.onNotice =
function my_notice (e)
{

    e.user.display (e.meat, "NOTICE", e.user.nick);
    
}

CIRCChannel.prototype.onCTCPAction =
function my_caction (e)
{

    e.user.display (e.CTCPData, "ACTION");

}

CIRCChannel.prototype.onJoin =
function my_cjoin (e)
{

    this.display(e.user.properNick + " has joined " + e.channel.name,
                 "JOIN");
    this.list.add (e.user.getDecoratedNick());
    
}

CIRCChannel.prototype.onPart =
function my_cpart (e)
{

    this.display (e.user.properNick + " has left " + e.channel.name, "PART");
    this.list.remove (e.user.getDecoratedNick());
    
}

CIRCChannel.prototype.onKick =
function my_ckick (e)
{

    this.display (e.lamer.properNick + " was booted from " + e.channel.name +
                  " by " + e.user.properNick + " (" + e.reason + ")", "KICK");
    this.list.listContainer.removeChild (e.user.getDecoratedNick());

}

CIRCChannel.prototype.onNick =
function my_cnick (e)
{

    this.display (e.oldNick + " is now known as " + e.user.properNick, "NICK");
    e.user.updateDecoratedNick();
    
}


CIRCChannel.prototype.onQuit =
function my_cquit (e)
{

    this.display (e.user.properNick + " has left " + e.server.parent.name +
                  " (" + e.reason + ")", "QUIT");
    this.list.listContainer.removeChild (e.user.getDecoratedNick());
    
}

CIRCUser.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    
    this.display (e.meat, "PRIVMSG");
    
}

CIRCUser.prototype.onNotice =
function my_notice (e)
{

    this.display (e.meat, "NOTICE");
    
}
