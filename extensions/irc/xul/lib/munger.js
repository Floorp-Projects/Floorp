function CMungerEntry (name, regex, className, tagName)
{
    
    this.name = name;
    this.tagName = (tagName) ? tagName : "html:span";

    if (regex instanceof RegExp)
        this.regex = regex;
    else 
        this.lambdaMatch = regex;

    if (typeof className == "function")
        this.lambdaReplace = className;
    else 
        this.className = className;
    
}

function CMunger () 
{
    
    this.entries = new Object();
    
}

CMunger.prototype.addRule =
function mng_addrule (name, regex, className)
{
    
    this.entries[name] = new CMungerEntry (name, regex, className);
    
}

CMunger.prototype.delRule =
function mng_delrule (name)
{

    delete this.entries[name];
    
}

CMunger.prototype.munge =
function mng_munge (text, containerTag, eventDetails)
{
    var entry;
    var ary;
    
    if (!containerTag)
        containerTag = document.createElement (tagName);
 
    for (entry in this.entries)
    {
        if (typeof this.entries[entry].lambdaMatch == "function")
        {
            var rval;
            rval = this.entries[entry].lambdaMatch(text, containerTag,
                                                   eventDetails,
                                                   this.entries[entry]);
            if (rval)
                ary = [(void 0), rval];
            else
                ary = null;
        }
        else
            ary = text.match(this.entries[entry].regex);
        
        if (ary != null)
        {
            var startPos = text.indexOf(ary[1]);
            
            if (typeof this.entries[entry].lambdaReplace == "function")
            {
                this.munge (text.substr(0,startPos), containerTag,
                            eventDetails);
                this.entries[entry].lambdaReplace (ary[1], containerTag,
                                                   eventDetails,
                                                   this.entries[entry]);
                this.munge (text.substr (startPos + ary[1].length, text.length),
                            containerTag, eventDetails);
                
                return containerTag;
            }
            else
            {
                this.munge (text.substr(0,startPos), containerTag, eventDetails);

                var subTag = document.createElement
                    (this.entries[entry].tagName);

                subTag.setAttribute ("class", this.entries[entry].className);
                subTag.appendChild
                    (document.createTextNode (ary[1]));
                containerTag.appendChild (subTag);
                this.munge (text.substr (startPos + ary[1].length, text.length),
                            containerTag, eventDetails);

                return containerTag;
            }
        }        
    }

    containerTag.appendChild (document.createTextNode (text));
    return containerTag;
    
}
