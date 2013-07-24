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

function setAllTextValuesTo(newText)
{
    for (var i = 0; ; ++i) {
        theElement = document.getElementById("set" + i);
        if (!theElement) {
            break;
        }
        if (theElement.tagName == "INPUT" ||
            theElement.tagName == "TEXTAREA") {
            theElement.value = newText;
        } else {
            theElement.firstChild.textContent = newText;
        }
    }
}

function setAllTextDefaultValuesTo(newText)
{
    for (var i = 0; ; ++i) {
        theElement = document.getElementById("set" + i);
        if (!theElement) {
            break;
        }
        if (theElement.tagName == "INPUT" ||
            theElement.tagName == "TEXTAREA") {
            theElement.defaultValue = newText;
        } else {
            theElement.firstChild.textContent = newText;
        }
    }
}

function setAllTextChildrenTo(newText)
{
    for (var i = 0; ; ++i) {
        theElement = document.getElementById("set" + i);
        if (!theElement) {
            break;
        }
        if (theElement.tagName == "INPUT") {
            theElement.value = newText;
        } else {
            theElement.firstChild.textContent = newText;
        }
    }
}

function appendTextFromArray(textArray)
{
    for (var i = 0; ; ++i) {
        theElement = document.getElementById("set" + i);
        if (!theElement) {
            break;
        }
        for (var j = 0; j < textArray.length; ++j) {
            if (theElement.tagName == "INPUT") {
                theElement.value += textArray[j];
            } else {
                var textNode = document.createTextNode(textArray[j]);
                theElement.appendChild(textNode);
            }
        }
    }
}

function appendSpansFromArray(textArray)
{
    for (var i = 0; ; ++i) {
        theElement = document.getElementById("set" + i);
        if (!theElement) {
            break;
        }
        for (var j = 0; j < textArray.length; ++j) {
            // fake the result for elements that can't have markup content
            if (theElement.tagName == "INPUT") {
                theElement.value += textArray[j];
            } else if (theElement.tagName == "TEXTAREA") {
                theElement.innerHTML += textArray[j];
            } else {
                var span = document.createElement("span");
                span.innerHTML = textArray[j];
                theElement.appendChild(span);
            }
        }
    }
}

function prependTextFromArray(textArray)
{
    for (var i = 0; ; ++i) {
        theElement = document.getElementById("set" + i);
        if (!theElement) {
            break;
        }
        for (var j = 0; j < textArray.length; ++j) {
            if (theElement.tagName == "INPUT") {
                theElement.value = textArray[j] + theElement.value;
            } else {
                var textNode = document.createTextNode(textArray[j]);
                theElement.insertBefore(textNode, theElement.firstChild);
            }
        }
    }
}

function prependSpansFromArray(textArray)
{
    for (var i = 0; ; ++i) {
        theElement = document.getElementById("set" + i);
        if (!theElement) {
            break;
        }
        for (var j = 0; j < textArray.length; ++j) {
            // fake the result for elements that can't have markup content
            if (theElement.tagName == "INPUT") {
                theElement.value = textArray[j] + theElement.value;
            } else if (theElement.tagName == "TEXTAREA") {
                theElement.innerHTML = textArray[j] + theElement.innerHTML;
            } else {
                var span = document.createElement("span");
                span.innerHTML = textArray[j];
                theElement.insertBefore(span, theElement.firstChild);
            }
        }
    }
}

function removeElements()
{
    for (var i = 0; ; ++i) {
        theElement = document.getElementById("set" + i);
        if (!theElement) {
            break;
        }
        theElement.parentNode.removeChild(theElement);
    }
}
