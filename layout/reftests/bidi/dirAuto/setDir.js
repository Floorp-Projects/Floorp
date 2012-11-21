function setAllDir(value)
{
    for (var i = 0; ; ++i) {
        try {
            theElement = document.getElementById("set" + i);
            theElement.dir = value;
        } catch(e) {
            break;
        }
    }
}

function setAllDirAttribute(value)
{
    for (var i = 0; ; ++i) {
	try {
	    theElement = document.getElementById("set" + i);
	    theElement.setAttribute("dir", value);
	} catch(e) {
	    break;
	}
    }
}

function removeAllDirAttribute()
{
    for (var i = 0; ; ++i) {
	try {
	    theElement = document.getElementById("set" + i);
	    theElement.removeAttribute("dir");
	} catch(e) {
	    break;
	}
    }
}

function addOneElement(innerHTML)
{
    var container = document.getElementById("container");
    var elem = document.createElement("div");
    elem.innerHTML = innerHTML;
    container.appendChild(elem);
}

function addLTRAutoElements()
{
    addOneElement('<input type="text" value="ABC אבג" id="set0" dir="auto">');
    addOneElement('<span id="set1" dir="auto">ABC אבג</span>');
    addOneElement('<textarea id="set2" dir="auto">ABC אבג</textarea>');
    addOneElement('<button id="set3" dir="auto">ABC אבג</button>');
    addOneElement('<bdi id="set4">ABC אבג</bdi>');
}

function addRTLAutoElements()
{
    addOneElement('<input type="text" value="אבג ABC" id="set0" dir="auto">');
    addOneElement('<span id="set1" dir="auto">אבג ABC</span>');
    addOneElement('<textarea id="set2" dir="auto">אבג ABC</textarea>');
    addOneElement('<button id="set3" dir="auto">אבג ABC</button>');
    addOneElement('<bdi id="set4">אבג ABC</bdi>');
}
