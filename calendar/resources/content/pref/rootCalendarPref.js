// Preferences observer object; implements nsIObserver
function calendarPrefObserver( CalendarPreferences )
{
   this.CalendarPreferences = CalendarPreferences;
   try {
     var pbi = rootPrefNode.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
     pbi.addObserver("calendar", this, false);
  } catch(ex) {
    dump("Calendar: Failed to observe prefs: " + ex + "\n");
  }
}

calendarPrefObserver.prototype =
{
  domain: "calendar.",
  observe: function(subject, topic, prefName)
  {
     // when calendar pref was changed, we reinitialize 
     this.CalendarPreferences.loadPreferences();

     //this causes Mozilla to freeze
     //firePendingEvents(); 
  }
}

function removePrefObserver( e ){ // this one is called on window destruction (by unload listener from mozgestOverlay.xul)
  
   dump( "\n"+e.target.localName );
   if (e.target.localName == null){ // test if the window is really closing
      dump( "\nremove CALENDAR prefs Observer" );
      var pbi = rootPrefNode.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
      pbi.removeObserver(window.calendarPrefObserver.domain, window.calendarPrefObserver.calendarPrefObserver);
   }
}

window.addEventListener("unload", removePrefObserver, true); 



function calendarPreferences( CalendarWindow )
{
   window.calendarPrefObserver = new calendarPrefObserver( this );
   
   this.calendarWindow = CalendarWindow;

   this.arrayOfPrefs = new Object();
   this.arrayOfPrefs.showalarms = true;
   
   this.calendarPref = prefService.getBranch("calendar."); // preferences mozgest node
   
   // read prefs or set Defaults on the first run
  try {
    this.loadPreferences();  
  }
  catch(e) {
     this.calendarPref.setBoolPref("alarms.show", true);
     this.calendarPref.setBoolPref("alarms.playsound", false);
    
     this.loadPreferences();
  }
  
  
}

calendarPreferences.prototype.loadPreferences = function()
{
   this.arrayOfPrefs.showalarms = this.calendarPref.getBoolPref( "alarms.show" );

   this.arrayOfPrefs.alarmsplaysound = this.calendarPref.getBoolPref( "alarms.playsound" );
}

calendarPreferences.prototype.getPref = function( Preference )
{
   var ThisPref = eval( "this.arrayOfPrefs."+Preference );

   return( ThisPref );
}
