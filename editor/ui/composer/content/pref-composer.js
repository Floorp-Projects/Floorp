function setColorWell(menu,otherId,setbackground)
  {
    // Find the colorWell and colorPicker in the hierarchy.
    var colorWell = menu.firstChild;
    var colorPicker = menu.firstChild.nextSibling.nextSibling.firstChild;
    var colorRef = menu.nextSibling;                // colour value is held here
    
    // Extract color from colorPicker and assign to colorWell.
    var color = colorPicker.getAttribute('color');
    // set colour values in the display 
    setColorFromPicker(colorWell,color,otherId,setbackground);
    // set the colour value internally for use in prefwindow
    colorRef.setAttribute( "value", color );
  }

function getColorFromWellAndSetValue(menuid,otherId,setbackground)
  {
    var menu      = document.getElementById( menuid );  // picker container
    var colorWell = menu.firstChild;                  // display for picker colour
    var colorRef  = menu.nextSibling;                 // prefs JS sets this.
    colorWell.style.backgroundColor = colorRef.getAttribute("value"); // set the well from prefs.
    var color     = colorWell.style.backgroundColor;   
    setColorFromPicker(null,color,otherId,setbackground);
  }     

function setColorFromPicker(colorWell,color,otherId,setbackground)
  {
    if (colorWell)
      colorWell.style.backgroundColor = color;
    if (otherId) 
      {
  	    otherElement = document.getElementById( otherId );
	      if (setbackground) 
          {
		        var basestyle = otherElement.getAttribute('basestyle');
	  	      otherElement.setAttribute("style", basestyle + "background-color: " + color + ";");
	        }
	      else
		      otherElement.setAttribute("style", "color: " + color + ";" );
      }
  }
  
function Startup()
  {
    getColorFromWellAndSetValue("textMenu", "normaltext", false);
    getColorFromWellAndSetValue("linkMenu", "linktext", false);
    getColorFromWellAndSetValue("aLinkMenu", "activelinktext", false);
    getColorFromWellAndSetValue("fLinkMenu", "followedlinktext", false);
    getColorFromWellAndSetValue("backgroundMenu", "backgroundcolor", true);
    if( document.getElementById( "useCustomColors" ).data == 0 )
      useNavigatorColors();
    return true;
  }                   
  
function useNavigatorColors()
  {
    // use colors from navigator
    var prefstrings = ["browser.display.foreground_color", "browser.anchor_color", "browser.visited_color", "browser.display.background_color"];
    var dataEls = ["text", "link", "fLink", "background"];
    for( var i = 0; i < dataEls.length; i++ )
      {
        var element = document.getElementById( ( dataEls[i] + "Data" ) );
        var navigatorValue = parent.hPrefWindow.getPref( "color", prefstrings[i] );
        element.setAttribute( "value", navigatorValue );
        element = document.getElementById( ( dataEls[i] + "Label" ) );
        element.setAttribute( "disabled", "true" );
        element = document.getElementById( ( dataEls[i] + "Menu" ) );
        element.setAttribute( "disabled", "true" );
      }
    document.getElementById( "aLinkLabel" ).setAttribute( "disabled", "true" );
    document.getElementById( "aLinkMenu" ).setAttribute( "disabled", "true" );
    document.getElementById( "useCustomColors" ).setAttribute( "disabled", "true" );
    getColorFromWellAndSetValue("textMenu", "normaltext", false);
    getColorFromWellAndSetValue("linkMenu", "linktext", false);
    getColorFromWellAndSetValue("aLinkMenu", "activelinktext", false);
    getColorFromWellAndSetValue("fLinkMenu", "followedlinktext", false);
    getColorFromWellAndSetValue("backgroundMenu", "backgroundcolor", true);
  }

function useCustomColors()
  {
    var dataEls = ["text", "link", "fLink", "background"];
    for( var i = 0; i < dataEls.length; i++ )
      {
        var element = document.getElementById( ( dataEls[i] + "Data" ) );
        element = document.getElementById( ( dataEls[i] + "Label" ) );
        element.removeAttribute( "disabled" );
        element = document.getElementById( ( dataEls[i] + "Menu" ) );
        element.removeAttribute( "disabled" );
      }
    document.getElementById( "aLinkLabel" ).removeAttribute( "disabled" );
    document.getElementById( "aLinkMenu" ).removeAttribute( "disabled" );
    document.getElementById( "useCustomColors" ).removeAttribute( "disabled" );
  }
  
function useDefaultColors()
  {
    // use default colors from editor.js
    var elIds = ["textData", "linkData", "aLinkData", "fLinkData", "backgroundData"];
    for( var i = 0; i < elIds.length; i++ )
      {
        var element = document.getElementById( elIds[i] );
        var defPref = parent.hPrefWindow.getPref( element.getAttribute( "preftype" ), element.getAttribute( "prefstring" ), true );
        element.setAttribute( "value", defPref );
      }
    Startup();
  }
  
  
  
  
  