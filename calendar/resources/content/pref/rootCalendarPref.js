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
     
     switch( prefName )
     {
        case "calendar.week.start":
         this.CalendarPreferences.calendarWindow.currentView.refresh();
        break

        case "calendar.date.format":
         this.CalendarPreferences.calendarWindow.currentView.refresh();
         unifinderRefesh();
        default:
        break;

     }
     
     //this causes Mozilla to freeze
     //firePendingEvents(); 
  }
}

function calendarPreferences( CalendarWindow )
{
   window.calendarPrefObserver = new calendarPrefObserver( this );
   
   this.calendarWindow = CalendarWindow;

   this.arrayOfPrefs = new Object();
   
   this.calendarPref = prefService.getBranch("calendar."); // preferences mozgest node
   
   // read prefs or set Defaults on the first run
  try {
    this.loadPreferences();  
  }
  catch(e) {
     this.calendarPref.setBoolPref( "alarms.show", true );
     this.calendarPref.setBoolPref( "alarms.playsound", false );
     this.calendarPref.setIntPref( "date.format", 0 );
     this.calendarPref.setIntPref( "week.start", 0 );
     this.calendarPref.setIntPref( "event.defaultlength", 60 );
     this.calendarPref.setIntPref( "alarms.defaultsnoozelength", 10 );
     this.loadPreferences();
  }
  
  
}

calendarPreferences.prototype.loadPreferences = function()
{
   this.arrayOfPrefs.showalarms = this.calendarPref.getBoolPref( "alarms.show" );

   this.arrayOfPrefs.alarmsplaysound = this.calendarPref.getBoolPref( "alarms.playsound" );

   this.arrayOfPrefs.dateformat = this.calendarPref.getIntPref( "date.format" );
   
   this.arrayOfPrefs.weekstart = this.calendarPref.getIntPref( "week.start" );

   this.arrayOfPrefs.defaulteventlength = this.calendarPref.getIntPref( "event.defaultlength" );

   this.arrayOfPrefs.defaultsnoozelength = this.calendarPref.getIntPref( "alarms.defaultsnoozelength" );
}

calendarPreferences.prototype.getPref = function( Preference )
{
   var ThisPref = eval( "this.arrayOfPrefs."+Preference );

   return( ThisPref );
}
