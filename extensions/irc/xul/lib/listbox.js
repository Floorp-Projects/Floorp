function CListBox ()
{

    this.listContainer = document.createElement ("html:a");
    this.listContainer.setAttribute ("class", "list");

}

CListBox.prototype.clear =
function lbox_clear (e)
{
    var obj = this.listContainer.firstChild;

    while (obj)
    {
        this.listContainer.removeChild (obj);
        obj = obj.nextSibling;    
    }    

}

CListBox.prototype.clickHandler =
function lbox_chandler (e)
{
    var lm = this.listManager;
    
    e.target = this;
    if (lm && typeof lm.onClick == "function")
        lm.onClick ({realTarget: this, event: e});
    
}   

CListBox.prototype.onClick =
function lbox_chandler (e)
{

    dd ("onclick: \n" + dumpObjectTree (e, 1));
    
}

CListBox.prototype.add =
function lbox_add (stuff, tag)
{
    var option = document.createElement ("html:a");
    option.setAttribute ("class", "list-option");

    option.appendChild (stuff);
    option.appendChild (document.createElement("html:br"));
    option.onclick = this.clickHandler;
    option.listManager = this;
    option.tag = tag;
    this.listContainer.appendChild (option);
    
}

CListBox.prototype.remove =
function lbox_remove (stuff)
{
    var option = this.listContainer.firstChild;

    while (option)
    {
        if (option.firstChild == stuff)
        {
            this.container.removeChild (option);
            return true;
        }
        option = option.nextSibling;
    }

    return false;
}

CListBox.prototype.enumerateElements =
function lbox_enum (callback)
{
    var i = 0;
    var current = this.listContainer.firstChild;
    
    while (current)
    {
        callback (current, i++);
        current = current.nextSibling;
    }
    
}
