/***************************************************************
* inFormManager -------------------------------------------------
*  Manages the reading and writing of forms via simple maps of
*  attribute/value pairs.  A "form" is simply a XUL window which
*  contains "form widgets" such as textboxes and menulists.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
* REQUIRED IMPORTS:
****************************************************************/

////////////////////////////////////////////////////////////////////////////
//// class inFormManager

var inFormManager =
{
  // Object
  readWindow: function(aWindow, aIds)
  {
    var el, fn;
    var doc = aWindow.document;
    var map = {};
    for (var i = 0; i < aIds.length; i++) {
      el = doc.getElementById(aIds[i]);
      if (el) {
        this.persistIf(doc, el);
        fn = this["read_"+el.localName];
        if (fn)
          map[aIds[i]] = fn(el);
      }
    }

    return map;
  },

  // void
  writeWindow: function(aWindow, aMap)
  {
    var el, fn;
    var doc = aWindow.document;
    for (var i = 0; i < aIds.length; i++) {
      el = doc.getElementById(aIds[i]);
      if (el) {
        fn = this["write_"+el.localName];
        if (fn)
          fn(el, aMap[aIds[i]]);
      }
    }
  },

  persistIf: function(aDoc, aEl)
  {
    if (aEl.getAttribute("persist") == "true" && aEl.hasAttribute("id")) {
      aEl.setAttribute("value", aEl.value);
      aDoc.persist(aEl.getAttribute("id"), "value");
    }
  },

  read_textbox: function(aEl)
  {
    return aEl.value;
  },

  read_menulist: function(aEl)
  {
    return aEl.getAttribute("value");
  },

  read_checkbox: function(aEl)
  {
    return aEl.getAttribute("checked") == "true";
  },

  read_radiogroup: function(aEl)
  {
    return aEl.getAttribute("value");
  },

  read_colorpicker: function(aEl)
  {
    return aEl.getAttribute("color");
  },

  write_textbox: function(aEl, aValue)
  {
    aEl.setAttribute("value", aValue);
  },

  write_menulist: function(aEl, aValue)
  {
    aEl.setAttribute("value", aValue);
  },

  write_checkbox: function(aEl, aValue)
  {
    aEl.setAttribute("checked", aValue);
  },

  write_radiogroup: function(aEl, aValue)
  {
    aEl.setAttribute("value", aValue);
  },

  write_colorpicker: function(aEl, aValue)
  {
    aEl.color = aValue;
  }
};

