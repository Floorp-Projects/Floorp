function arrayExists(array, x) {
    for (var i = 0; i < array.length; i++) {
        if (array[i] == x) return true;
    }
    return false;
}

Date.prototype.formatDate = function (input,time) {
    // formatDate :
    // a PHP date like function, for formatting date strings
    // See: http://www.php.net/date
    //
    // input : format string
    // time : epoch time (seconds, and optional)
    //
    // if time is not passed, formatting is based on 
    // the current "this" date object's set time.
    //
    // supported:
    // a, A, B, d, D, F, g, G, h, H, i, j, l (lowercase L), L, 
    // m, M, n, O, r, s, S, t, U, w, W, y, Y, z
    //
    // unsupported:
    // I (capital i), T, Z    

    var switches =    ["a", "A", "B", "d", "D", "F", "g", "G", "h", "H", 
                       "i", "j", "l", "L", "m", "M", "n", "O", "r", "s", 
                       "S", "t", "U", "w", "W", "y", "Y", "z"];
    var daysLong =    ["Sunday", "Monday", "Tuesday", "Wednesday", 
                       "Thursday", "Friday", "Saturday"];
    var daysShort =   ["Sun", "Mon", "Tue", "Wed", 
                       "Thu", "Fri", "Sat"];
    var monthsShort = ["Jan", "Feb", "Mar", "Apr",
                       "May", "Jun", "Jul", "Aug", "Sep",
                       "Oct", "Nov", "Dec"];
    var monthsLong =  ["January", "February", "March", "April",
                       "May", "June", "July", "August", "September",
                       "October", "November", "December"];
    var daysSuffix = ["st", "nd", "rd", "th", "th", "th", "th", // 1st - 7th
                      "th", "th", "th", "th", "th", "th", "th", // 8th - 14th
                      "th", "th", "th", "th", "th", "th", "st", // 15th - 21st
                      "nd", "rd", "th", "th", "th", "th", "th", // 22nd - 28th
                      "th", "th", "st"];                        // 29th - 31st

    function a() {
        // Lowercase Ante meridiem and Post meridiem
        return self.getHours() > 11? "pm" : "am";
    }
    function A() {
        // Uppercase Ante meridiem and Post meridiem
        return self.getHours() > 11? "PM" : "AM";
    }

    function B(){
        // Swatch internet time. code simply grabbed from ppk,
        // since I was feeling lazy:
        // http://www.xs4all.nl/~ppk/js/beat.html
        var off = (self.getTimezoneOffset() + 60)*60;
        var theSeconds = (self.getHours() * 3600) + 
                         (self.getMinutes() * 60) + 
                          self.getSeconds() + off;
        var beat = Math.floor(theSeconds/86.4);
        if (beat > 1000) beat -= 1000;
        if (beat < 0) beat += 1000;
        if ((""+beat).length == 1) beat = "00"+beat;
        if ((""+beat).length == 2) beat = "0"+beat;
        return beat;
    }
    
    function d() {
        // Day of the month, 2 digits with leading zeros
        return new String(self.getDate()).length == 1?
        "0"+self.getDate() : self.getDate();
    }
    function D() {
        // A textual representation of a day, three letters
        return daysShort[self.getDay()];
    }
    function F() {
        // A full textual representation of a month
        return monthsLong[self.getMonth()];
    }
    function g() {
        // 12-hour format of an hour without leading zeros
        return self.getHours() > 12? self.getHours()-12 : self.getHours();
    }
    function G() {
        // 24-hour format of an hour without leading zeros
        return self.getHours();
    }
    function h() {
        // 12-hour format of an hour with leading zeros
        if (self.getHours() > 12) {
          var s = new String(self.getHours()-12);
          return s.length == 1?
          "0"+ (self.getHours()-12) : self.getHours()-12;
        } else { 
          var s = new String(self.getHours());
          return s.length == 1?
          "0"+self.getHours() : self.getHours();
        }  
    }
    function H() {
        // 24-hour format of an hour with leading zeros
        return new String(self.getHours()).length == 1?
        "0"+self.getHours() : self.getHours();
    }
    function i() {
        // Minutes with leading zeros
        return new String(self.getMinutes()).length == 1? 
        "0"+self.getMinutes() : self.getMinutes(); 
    }
    function j() {
        // Day of the month without leading zeros
        return self.getDate();
    }    
    function l() {
        // A full textual representation of the day of the week
        return daysLong[self.getDay()];
    }
    function L() {
        // leap year or not. 1 if leap year, 0 if not.
        // the logic should match iso's 8601 standard.
        var y_ = Y();
        if (         
            (y_ % 4 == 0 && y_ % 100 != 0) ||
            (y_ % 4 == 0 && y_ % 100 == 0 && y_ % 400 == 0)
            ) {
            return 1;
        } else {
            return 0;
        }
    }
    function m() {
        // Numeric representation of a month, with leading zeros
        return self.getMonth() < 9?
        "0"+(self.getMonth()+1) : 
        self.getMonth()+1;
    }
    function M() {
        // A short textual representation of a month, three letters
        return monthsShort[self.getMonth()];
    }
    function n() {
        // Numeric representation of a month, without leading zeros
        return self.getMonth()+1;
    }
    function O() {
        // Difference to Greenwich time (GMT) in hours
        var os = Math.abs(self.getTimezoneOffset());
        var h = ""+Math.floor(os/60);
        var m = ""+(os%60);
        h.length == 1? h = "0"+h:1;
        m.length == 1? m = "0"+m:1;
        return self.getTimezoneOffset() < 0 ? "+"+h+m : "-"+h+m;
    }
    function r() {
        // RFC 822 formatted date
        var r; // result
        //  Thu    ,     21          Dec         2000
        r = D() + ", " + j() + " " + M() + " " + Y() +
        //        16     :    01     :    07          +0200
            " " + H() + ":" + i() + ":" + s() + " " + O();
        return r;
    }
    function S() {
        // English ordinal suffix for the day of the month, 2 characters
        return daysSuffix[self.getDate()-1];
    }
    function s() {
        // Seconds, with leading zeros
        return new String(self.getSeconds()).length == 1?
        "0"+self.getSeconds() : self.getSeconds();
    }
    function t() {

        // thanks to Matt Bannon for some much needed code-fixes here!
        var daysinmonths = [null,31,28,31,30,31,30,31,31,30,31,30,31];
        if (L()==1 && n()==2) return 29; // leap day
        return daysinmonths[n()];
    }
    function U() {
        // Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)
        return Math.round(self.getTime()/1000);
    }
    function W() {
        // Weeknumber, as per ISO specification:
        // http://www.cl.cam.ac.uk/~mgk25/iso-time.html
        
        // if the day is three days before newyears eve,
        // there's a chance it's "week 1" of next year.
        // here we check for that.
        var beforeNY = 364+L() - z();
        var afterNY  = z();
        var weekday = w()!=0?w()-1:6; // makes sunday (0), into 6.
        if (beforeNY <= 2 && weekday <= 2-beforeNY) {
            return 1;
        }
        // similarly, if the day is within threedays of newyears
        // there's a chance it belongs in the old year.
        var ny = new Date("January 1 " + Y() + " 00:00:00");
        var nyDay = ny.getDay()!=0?ny.getDay()-1:6;
        if (
            (afterNY <= 2) && 
            (nyDay >=4)  && 
            (afterNY >= (6-nyDay))
            ) {
            // Since I'm not sure we can just always return 53,
            // i call the function here again, using the last day
            // of the previous year, as the date, and then just
            // return that week.
            var prevNY = new Date("December 31 " + (Y()-1) + " 00:00:00");
            return prevNY.formatDate("W");
        }
        
        // week 1, is the week that has the first thursday in it.
        // note that this value is not zero index.
        if (nyDay <= 3) {
            // first day of the year fell on a thursday, or earlier.
            return 1 + Math.floor( ( z() + nyDay ) / 7 );
        } else {
            // first day of the year fell on a friday, or later.
            return 1 + Math.floor( ( z() - ( 7 - nyDay ) ) / 7 );
        }
    }
    function w() {
        // Numeric representation of the day of the week
        return self.getDay();
    }
    
    function Y() {
        // A full numeric representation of a year, 4 digits

        // we first check, if getFullYear is supported. if it
        // is, we just use that. ppks code is nice, but wont
        // work with dates outside 1900-2038, or something like that
        if (self.getFullYear) {
            var newDate = new Date("January 1 2001 00:00:00 +0000");
            var x = newDate .getFullYear();
            if (x == 2001) {              
                // i trust the method now
                return self.getFullYear();
            }
        }
        // else, do this:
        // codes thanks to ppk:
        // http://www.xs4all.nl/~ppk/js/introdate.html
        var x = self.getYear();
        var y = x % 100;
        y += (y < 38) ? 2000 : 1900;
        return y;
    }
    function y() {
        // A two-digit representation of a year
        var y = Y()+"";
        return y.substring(y.length-2,y.length);
    }
    function z() {
        // The day of the year, zero indexed! 0 through 366
        var t = new Date("January 1 " + Y() + " 00:00:00");
        var diff = self.getTime() - t.getTime();
        return Math.floor(diff/1000/60/60/24);
    }
        
    var self = this;
    if (time) {
        // save time
        var prevTime = self.getTime();
        self.setTime(time);
    }
    
    var ia = input.split("");
    var ij = 0;
    while (ia[ij]) {
        if (ia[ij] == "\\") {
            // this is our way of allowing users to escape stuff
            ia.splice(ij,1);
        } else {
            if (arrayExists(switches,ia[ij])) {
                ia[ij] = eval(ia[ij] + "()");
            }
        }
        ij++;
    }
    // reset time, back to what it was
    if (prevTime) {
        self.setTime(prevTime);
    }
    return ia.join("");
}

var date = new Date("1/1/2007 1:11:11");

var ret = "";
for (i = 0; i < 500; ++i) {
    var shortFormat = date.formatDate("Y-m-d");
    var longFormat = date.formatDate("l, F d, Y g:i:s A");
    ret += shortFormat + longFormat;
    date.setTime(date.getTime() + 84266956);
}
var expected = "2007-01-01Monday, January 01, 2007 1:11:11 AM2007-01-02Tuesday, January 02, 2007 0:35:37 AM2007-01-03Wednesday, January 03, 2007 0:00:04 AM2007-01-03Wednesday, January 03, 2007 11:24:31 PM2007-01-04Thursday, January 04, 2007 10:48:58 PM2007-01-05Friday, January 05, 2007 10:13:25 PM2007-01-06Saturday, January 06, 2007 9:37:52 PM2007-01-07Sunday, January 07, 2007 9:02:19 PM2007-01-08Monday, January 08, 2007 8:26:46 PM2007-01-09Tuesday, January 09, 2007 7:51:13 PM2007-01-10Wednesday, January 10, 2007 7:15:40 PM2007-01-11Thursday, January 11, 2007 6:40:07 PM2007-01-12Friday, January 12, 2007 6:04:34 PM2007-01-13Saturday, January 13, 2007 5:29:01 PM2007-01-14Sunday, January 14, 2007 4:53:28 PM2007-01-15Monday, January 15, 2007 4:17:55 PM2007-01-16Tuesday, January 16, 2007 3:42:22 PM2007-01-17Wednesday, January 17, 2007 3:06:49 PM2007-01-18Thursday, January 18, 2007 2:31:16 PM2007-01-19Friday, January 19, 2007 1:55:43 PM2007-01-20Saturday, January 20, 2007 1:20:10 PM2007-01-21Sunday, January 21, 2007 12:44:37 PM2007-01-22Monday, January 22, 2007 12:09:04 PM2007-01-23Tuesday, January 23, 2007 11:33:30 AM2007-01-24Wednesday, January 24, 2007 10:57:57 AM2007-01-25Thursday, January 25, 2007 10:22:24 AM2007-01-26Friday, January 26, 2007 9:46:51 AM2007-01-27Saturday, January 27, 2007 9:11:18 AM2007-01-28Sunday, January 28, 2007 8:35:45 AM2007-01-29Monday, January 29, 2007 8:00:12 AM2007-01-30Tuesday, January 30, 2007 7:24:39 AM2007-01-31Wednesday, January 31, 2007 6:49:06 AM2007-02-01Thursday, February 01, 2007 6:13:33 AM2007-02-02Friday, February 02, 2007 5:38:00 AM2007-02-03Saturday, February 03, 2007 5:02:27 AM2007-02-04Sunday, February 04, 2007 4:26:54 AM2007-02-05Monday, February 05, 2007 3:51:21 AM2007-02-06Tuesday, February 06, 2007 3:15:48 AM2007-02-07Wednesday, February 07, 2007 2:40:15 AM2007-02-08Thursday, February 08, 2007 2:04:42 AM2007-02-09Friday, February 09, 2007 1:29:09 AM2007-02-10Saturday, February 10, 2007 0:53:36 AM2007-02-11Sunday, February 11, 2007 0:18:03 AM2007-02-11Sunday, February 11, 2007 11:42:30 PM2007-02-12Monday, February 12, 2007 11:06:57 PM2007-02-13Tuesday, February 13, 2007 10:31:24 PM2007-02-14Wednesday, February 14, 2007 9:55:50 PM2007-02-15Thursday, February 15, 2007 9:20:17 PM2007-02-16Friday, February 16, 2007 8:44:44 PM2007-02-17Saturday, February 17, 2007 8:09:11 PM2007-02-18Sunday, February 18, 2007 7:33:38 PM2007-02-19Monday, February 19, 2007 6:58:05 PM2007-02-20Tuesday, February 20, 2007 6:22:32 PM2007-02-21Wednesday, February 21, 2007 5:46:59 PM2007-02-22Thursday, February 22, 2007 5:11:26 PM2007-02-23Friday, February 23, 2007 4:35:53 PM2007-02-24Saturday, February 24, 2007 4:00:20 PM2007-02-25Sunday, February 25, 2007 3:24:47 PM2007-02-26Monday, February 26, 2007 2:49:14 PM2007-02-27Tuesday, February 27, 2007 2:13:41 PM2007-02-28Wednesday, February 28, 2007 1:38:08 PM2007-03-01Thursday, March 01, 2007 1:02:35 PM2007-03-02Friday, March 02, 2007 12:27:02 PM2007-03-03Saturday, March 03, 2007 11:51:29 AM2007-03-04Sunday, March 04, 2007 11:15:56 AM2007-03-05Monday, March 05, 2007 10:40:23 AM2007-03-06Tuesday, March 06, 2007 10:04:50 AM2007-03-07Wednesday, March 07, 2007 9:29:17 AM2007-03-08Thursday, March 08, 2007 8:53:44 AM2007-03-09Friday, March 09, 2007 8:18:10 AM2007-03-10Saturday, March 10, 2007 7:42:37 AM2007-03-11Sunday, March 11, 2007 8:07:04 AM2007-03-12Monday, March 12, 2007 7:31:31 AM2007-03-13Tuesday, March 13, 2007 6:55:58 AM2007-03-14Wednesday, March 14, 2007 6:20:25 AM2007-03-15Thursday, March 15, 2007 5:44:52 AM2007-03-16Friday, March 16, 2007 5:09:19 AM2007-03-17Saturday, March 17, 2007 4:33:46 AM2007-03-18Sunday, March 18, 2007 3:58:13 AM2007-03-19Monday, March 19, 2007 3:22:40 AM2007-03-20Tuesday, March 20, 2007 2:47:07 AM2007-03-21Wednesday, March 21, 2007 2:11:34 AM2007-03-22Thursday, March 22, 2007 1:36:01 AM2007-03-23Friday, March 23, 2007 1:00:28 AM2007-03-24Saturday, March 24, 2007 0:24:55 AM2007-03-24Saturday, March 24, 2007 11:49:22 PM2007-03-25Sunday, March 25, 2007 11:13:49 PM2007-03-26Monday, March 26, 2007 10:38:16 PM2007-03-27Tuesday, March 27, 2007 10:02:43 PM2007-03-28Wednesday, March 28, 2007 9:27:10 PM2007-03-29Thursday, March 29, 2007 8:51:37 PM2007-03-30Friday, March 30, 2007 8:16:03 PM2007-03-31Saturday, March 31, 2007 7:40:30 PM2007-04-01Sunday, April 01, 2007 7:04:57 PM2007-04-02Monday, April 02, 2007 6:29:24 PM2007-04-03Tuesday, April 03, 2007 5:53:51 PM2007-04-04Wednesday, April 04, 2007 5:18:18 PM2007-04-05Thursday, April 05, 2007 4:42:45 PM2007-04-06Friday, April 06, 2007 4:07:12 PM2007-04-07Saturday, April 07, 2007 3:31:39 PM2007-04-08Sunday, April 08, 2007 2:56:06 PM2007-04-09Monday, April 09, 2007 2:20:33 PM2007-04-10Tuesday, April 10, 2007 1:45:00 PM2007-04-11Wednesday, April 11, 2007 1:09:27 PM2007-04-12Thursday, April 12, 2007 12:33:54 PM2007-04-13Friday, April 13, 2007 11:58:21 AM2007-04-14Saturday, April 14, 2007 11:22:48 AM2007-04-15Sunday, April 15, 2007 10:47:15 AM2007-04-16Monday, April 16, 2007 10:11:42 AM2007-04-17Tuesday, April 17, 2007 9:36:09 AM2007-04-18Wednesday, April 18, 2007 9:00:36 AM2007-04-19Thursday, April 19, 2007 8:25:03 AM2007-04-20Friday, April 20, 2007 7:49:30 AM2007-04-21Saturday, April 21, 2007 7:13:57 AM2007-04-22Sunday, April 22, 2007 6:38:23 AM2007-04-23Monday, April 23, 2007 6:02:50 AM2007-04-24Tuesday, April 24, 2007 5:27:17 AM2007-04-25Wednesday, April 25, 2007 4:51:44 AM2007-04-26Thursday, April 26, 2007 4:16:11 AM2007-04-27Friday, April 27, 2007 3:40:38 AM2007-04-28Saturday, April 28, 2007 3:05:05 AM2007-04-29Sunday, April 29, 2007 2:29:32 AM2007-04-30Monday, April 30, 2007 1:53:59 AM2007-05-01Tuesday, May 01, 2007 1:18:26 AM2007-05-02Wednesday, May 02, 2007 0:42:53 AM2007-05-03Thursday, May 03, 2007 0:07:20 AM2007-05-03Thursday, May 03, 2007 11:31:47 PM2007-05-04Friday, May 04, 2007 10:56:14 PM2007-05-05Saturday, May 05, 2007 10:20:41 PM2007-05-06Sunday, May 06, 2007 9:45:08 PM2007-05-07Monday, May 07, 2007 9:09:35 PM2007-05-08Tuesday, May 08, 2007 8:34:02 PM2007-05-09Wednesday, May 09, 2007 7:58:29 PM2007-05-10Thursday, May 10, 2007 7:22:56 PM2007-05-11Friday, May 11, 2007 6:47:23 PM2007-05-12Saturday, May 12, 2007 6:11:50 PM2007-05-13Sunday, May 13, 2007 5:36:17 PM2007-05-14Monday, May 14, 2007 5:00:43 PM2007-05-15Tuesday, May 15, 2007 4:25:10 PM2007-05-16Wednesday, May 16, 2007 3:49:37 PM2007-05-17Thursday, May 17, 2007 3:14:04 PM2007-05-18Friday, May 18, 2007 2:38:31 PM2007-05-19Saturday, May 19, 2007 2:02:58 PM2007-05-20Sunday, May 20, 2007 1:27:25 PM2007-05-21Monday, May 21, 2007 12:51:52 PM2007-05-22Tuesday, May 22, 2007 12:16:19 PM2007-05-23Wednesday, May 23, 2007 11:40:46 AM2007-05-24Thursday, May 24, 2007 11:05:13 AM2007-05-25Friday, May 25, 2007 10:29:40 AM2007-05-26Saturday, May 26, 2007 9:54:07 AM2007-05-27Sunday, May 27, 2007 9:18:34 AM2007-05-28Monday, May 28, 2007 8:43:01 AM2007-05-29Tuesday, May 29, 2007 8:07:28 AM2007-05-30Wednesday, May 30, 2007 7:31:55 AM2007-05-31Thursday, May 31, 2007 6:56:22 AM2007-06-01Friday, June 01, 2007 6:20:49 AM2007-06-02Saturday, June 02, 2007 5:45:16 AM2007-06-03Sunday, June 03, 2007 5:09:43 AM2007-06-04Monday, June 04, 2007 4:34:10 AM2007-06-05Tuesday, June 05, 2007 3:58:37 AM2007-06-06Wednesday, June 06, 2007 3:23:03 AM2007-06-07Thursday, June 07, 2007 2:47:30 AM2007-06-08Friday, June 08, 2007 2:11:57 AM2007-06-09Saturday, June 09, 2007 1:36:24 AM2007-06-10Sunday, June 10, 2007 1:00:51 AM2007-06-11Monday, June 11, 2007 0:25:18 AM2007-06-11Monday, June 11, 2007 11:49:45 PM2007-06-12Tuesday, June 12, 2007 11:14:12 PM2007-06-13Wednesday, June 13, 2007 10:38:39 PM2007-06-14Thursday, June 14, 2007 10:03:06 PM2007-06-15Friday, June 15, 2007 9:27:33 PM2007-06-16Saturday, June 16, 2007 8:52:00 PM2007-06-17Sunday, June 17, 2007 8:16:27 PM2007-06-18Monday, June 18, 2007 7:40:54 PM2007-06-19Tuesday, June 19, 2007 7:05:21 PM2007-06-20Wednesday, June 20, 2007 6:29:48 PM2007-06-21Thursday, June 21, 2007 5:54:15 PM2007-06-22Friday, June 22, 2007 5:18:42 PM2007-06-23Saturday, June 23, 2007 4:43:09 PM2007-06-24Sunday, June 24, 2007 4:07:36 PM2007-06-25Monday, June 25, 2007 3:32:03 PM2007-06-26Tuesday, June 26, 2007 2:56:30 PM2007-06-27Wednesday, June 27, 2007 2:20:56 PM2007-06-28Thursday, June 28, 2007 1:45:23 PM2007-06-29Friday, June 29, 2007 1:09:50 PM2007-06-30Saturday, June 30, 2007 12:34:17 PM2007-07-01Sunday, July 01, 2007 11:58:44 AM2007-07-02Monday, July 02, 2007 11:23:11 AM2007-07-03Tuesday, July 03, 2007 10:47:38 AM2007-07-04Wednesday, July 04, 2007 10:12:05 AM2007-07-05Thursday, July 05, 2007 9:36:32 AM2007-07-06Friday, July 06, 2007 9:00:59 AM2007-07-07Saturday, July 07, 2007 8:25:26 AM2007-07-08Sunday, July 08, 2007 7:49:53 AM2007-07-09Monday, July 09, 2007 7:14:20 AM2007-07-10Tuesday, July 10, 2007 6:38:47 AM2007-07-11Wednesday, July 11, 2007 6:03:14 AM2007-07-12Thursday, July 12, 2007 5:27:41 AM2007-07-13Friday, July 13, 2007 4:52:08 AM2007-07-14Saturday, July 14, 2007 4:16:35 AM2007-07-15Sunday, July 15, 2007 3:41:02 AM2007-07-16Monday, July 16, 2007 3:05:29 AM2007-07-17Tuesday, July 17, 2007 2:29:56 AM2007-07-18Wednesday, July 18, 2007 1:54:23 AM2007-07-19Thursday, July 19, 2007 1:18:50 AM2007-07-20Friday, July 20, 2007 0:43:16 AM2007-07-21Saturday, July 21, 2007 0:07:43 AM2007-07-21Saturday, July 21, 2007 11:32:10 PM2007-07-22Sunday, July 22, 2007 10:56:37 PM2007-07-23Monday, July 23, 2007 10:21:04 PM2007-07-24Tuesday, July 24, 2007 9:45:31 PM2007-07-25Wednesday, July 25, 2007 9:09:58 PM2007-07-26Thursday, July 26, 2007 8:34:25 PM2007-07-27Friday, July 27, 2007 7:58:52 PM2007-07-28Saturday, July 28, 2007 7:23:19 PM2007-07-29Sunday, July 29, 2007 6:47:46 PM2007-07-30Monday, July 30, 2007 6:12:13 PM2007-07-31Tuesday, July 31, 2007 5:36:40 PM2007-08-01Wednesday, August 01, 2007 5:01:07 PM2007-08-02Thursday, August 02, 2007 4:25:34 PM2007-08-03Friday, August 03, 2007 3:50:01 PM2007-08-04Saturday, August 04, 2007 3:14:28 PM2007-08-05Sunday, August 05, 2007 2:38:55 PM2007-08-06Monday, August 06, 2007 2:03:22 PM2007-08-07Tuesday, August 07, 2007 1:27:49 PM2007-08-08Wednesday, August 08, 2007 12:52:16 PM2007-08-09Thursday, August 09, 2007 12:16:43 PM2007-08-10Friday, August 10, 2007 11:41:10 AM2007-08-11Saturday, August 11, 2007 11:05:36 AM2007-08-12Sunday, August 12, 2007 10:30:03 AM2007-08-13Monday, August 13, 2007 9:54:30 AM2007-08-14Tuesday, August 14, 2007 9:18:57 AM2007-08-15Wednesday, August 15, 2007 8:43:24 AM2007-08-16Thursday, August 16, 2007 8:07:51 AM2007-08-17Friday, August 17, 2007 7:32:18 AM2007-08-18Saturday, August 18, 2007 6:56:45 AM2007-08-19Sunday, August 19, 2007 6:21:12 AM2007-08-20Monday, August 20, 2007 5:45:39 AM2007-08-21Tuesday, August 21, 2007 5:10:06 AM2007-08-22Wednesday, August 22, 2007 4:34:33 AM2007-08-23Thursday, August 23, 2007 3:59:00 AM2007-08-24Friday, August 24, 2007 3:23:27 AM2007-08-25Saturday, August 25, 2007 2:47:54 AM2007-08-26Sunday, August 26, 2007 2:12:21 AM2007-08-27Monday, August 27, 2007 1:36:48 AM2007-08-28Tuesday, August 28, 2007 1:01:15 AM2007-08-29Wednesday, August 29, 2007 0:25:42 AM2007-08-29Wednesday, August 29, 2007 11:50:09 PM2007-08-30Thursday, August 30, 2007 11:14:36 PM2007-08-31Friday, August 31, 2007 10:39:03 PM2007-09-01Saturday, September 01, 2007 10:03:30 PM2007-09-02Sunday, September 02, 2007 9:27:56 PM2007-09-03Monday, September 03, 2007 8:52:23 PM2007-09-04Tuesday, September 04, 2007 8:16:50 PM2007-09-05Wednesday, September 05, 2007 7:41:17 PM2007-09-06Thursday, September 06, 2007 7:05:44 PM2007-09-07Friday, September 07, 2007 6:30:11 PM2007-09-08Saturday, September 08, 2007 5:54:38 PM2007-09-09Sunday, September 09, 2007 5:19:05 PM2007-09-10Monday, September 10, 2007 4:43:32 PM2007-09-11Tuesday, September 11, 2007 4:07:59 PM2007-09-12Wednesday, September 12, 2007 3:32:26 PM2007-09-13Thursday, September 13, 2007 2:56:53 PM2007-09-14Friday, September 14, 2007 2:21:20 PM2007-09-15Saturday, September 15, 2007 1:45:47 PM2007-09-16Sunday, September 16, 2007 1:10:14 PM2007-09-17Monday, September 17, 2007 12:34:41 PM2007-09-18Tuesday, September 18, 2007 11:59:08 AM2007-09-19Wednesday, September 19, 2007 11:23:35 AM2007-09-20Thursday, September 20, 2007 10:48:02 AM2007-09-21Friday, September 21, 2007 10:12:29 AM2007-09-22Saturday, September 22, 2007 9:36:56 AM2007-09-23Sunday, September 23, 2007 9:01:23 AM2007-09-24Monday, September 24, 2007 8:25:49 AM2007-09-25Tuesday, September 25, 2007 7:50:16 AM2007-09-26Wednesday, September 26, 2007 7:14:43 AM2007-09-27Thursday, September 27, 2007 6:39:10 AM2007-09-28Friday, September 28, 2007 6:03:37 AM2007-09-29Saturday, September 29, 2007 5:28:04 AM2007-09-30Sunday, September 30, 2007 4:52:31 AM2007-10-01Monday, October 01, 2007 4:16:58 AM2007-10-02Tuesday, October 02, 2007 3:41:25 AM2007-10-03Wednesday, October 03, 2007 3:05:52 AM2007-10-04Thursday, October 04, 2007 2:30:19 AM2007-10-05Friday, October 05, 2007 1:54:46 AM2007-10-06Saturday, October 06, 2007 1:19:13 AM2007-10-07Sunday, October 07, 2007 0:43:40 AM2007-10-08Monday, October 08, 2007 0:08:07 AM2007-10-08Monday, October 08, 2007 11:32:34 PM2007-10-09Tuesday, October 09, 2007 10:57:01 PM2007-10-10Wednesday, October 10, 2007 10:21:28 PM2007-10-11Thursday, October 11, 2007 9:45:55 PM2007-10-12Friday, October 12, 2007 9:10:22 PM2007-10-13Saturday, October 13, 2007 8:34:49 PM2007-10-14Sunday, October 14, 2007 7:59:16 PM2007-10-15Monday, October 15, 2007 7:23:43 PM2007-10-16Tuesday, October 16, 2007 6:48:09 PM2007-10-17Wednesday, October 17, 2007 6:12:36 PM2007-10-18Thursday, October 18, 2007 5:37:03 PM2007-10-19Friday, October 19, 2007 5:01:30 PM2007-10-20Saturday, October 20, 2007 4:25:57 PM2007-10-21Sunday, October 21, 2007 3:50:24 PM2007-10-22Monday, October 22, 2007 3:14:51 PM2007-10-23Tuesday, October 23, 2007 2:39:18 PM2007-10-24Wednesday, October 24, 2007 2:03:45 PM2007-10-25Thursday, October 25, 2007 1:28:12 PM2007-10-26Friday, October 26, 2007 12:52:39 PM2007-10-27Saturday, October 27, 2007 12:17:06 PM2007-10-28Sunday, October 28, 2007 11:41:33 AM2007-10-29Monday, October 29, 2007 11:06:00 AM2007-10-30Tuesday, October 30, 2007 10:30:27 AM2007-10-31Wednesday, October 31, 2007 9:54:54 AM2007-11-01Thursday, November 01, 2007 9:19:21 AM2007-11-02Friday, November 02, 2007 8:43:48 AM2007-11-03Saturday, November 03, 2007 8:08:15 AM2007-11-04Sunday, November 04, 2007 6:32:42 AM2007-11-05Monday, November 05, 2007 5:57:09 AM2007-11-06Tuesday, November 06, 2007 5:21:36 AM2007-11-07Wednesday, November 07, 2007 4:46:03 AM2007-11-08Thursday, November 08, 2007 4:10:29 AM2007-11-09Friday, November 09, 2007 3:34:56 AM2007-11-10Saturday, November 10, 2007 2:59:23 AM2007-11-11Sunday, November 11, 2007 2:23:50 AM2007-11-12Monday, November 12, 2007 1:48:17 AM2007-11-13Tuesday, November 13, 2007 1:12:44 AM2007-11-14Wednesday, November 14, 2007 0:37:11 AM2007-11-15Thursday, November 15, 2007 0:01:38 AM2007-11-15Thursday, November 15, 2007 11:26:05 PM2007-11-16Friday, November 16, 2007 10:50:32 PM2007-11-17Saturday, November 17, 2007 10:14:59 PM2007-11-18Sunday, November 18, 2007 9:39:26 PM2007-11-19Monday, November 19, 2007 9:03:53 PM2007-11-20Tuesday, November 20, 2007 8:28:20 PM2007-11-21Wednesday, November 21, 2007 7:52:47 PM2007-11-22Thursday, November 22, 2007 7:17:14 PM2007-11-23Friday, November 23, 2007 6:41:41 PM2007-11-24Saturday, November 24, 2007 6:06:08 PM2007-11-25Sunday, November 25, 2007 5:30:35 PM2007-11-26Monday, November 26, 2007 4:55:02 PM2007-11-27Tuesday, November 27, 2007 4:19:29 PM2007-11-28Wednesday, November 28, 2007 3:43:56 PM2007-11-29Thursday, November 29, 2007 3:08:22 PM2007-11-30Friday, November 30, 2007 2:32:49 PM2007-12-01Saturday, December 01, 2007 1:57:16 PM2007-12-02Sunday, December 02, 2007 1:21:43 PM2007-12-03Monday, December 03, 2007 12:46:10 PM2007-12-04Tuesday, December 04, 2007 12:10:37 PM2007-12-05Wednesday, December 05, 2007 11:35:04 AM2007-12-06Thursday, December 06, 2007 10:59:31 AM2007-12-07Friday, December 07, 2007 10:23:58 AM2007-12-08Saturday, December 08, 2007 9:48:25 AM2007-12-09Sunday, December 09, 2007 9:12:52 AM2007-12-10Monday, December 10, 2007 8:37:19 AM2007-12-11Tuesday, December 11, 2007 8:01:46 AM2007-12-12Wednesday, December 12, 2007 7:26:13 AM2007-12-13Thursday, December 13, 2007 6:50:40 AM2007-12-14Friday, December 14, 2007 6:15:07 AM2007-12-15Saturday, December 15, 2007 5:39:34 AM2007-12-16Sunday, December 16, 2007 5:04:01 AM2007-12-17Monday, December 17, 2007 4:28:28 AM2007-12-18Tuesday, December 18, 2007 3:52:55 AM2007-12-19Wednesday, December 19, 2007 3:17:22 AM2007-12-20Thursday, December 20, 2007 2:41:49 AM2007-12-21Friday, December 21, 2007 2:06:16 AM2007-12-22Saturday, December 22, 2007 1:30:42 AM2007-12-23Sunday, December 23, 2007 0:55:09 AM2007-12-24Monday, December 24, 2007 0:19:36 AM2007-12-24Monday, December 24, 2007 11:44:03 PM2007-12-25Tuesday, December 25, 2007 11:08:30 PM2007-12-26Wednesday, December 26, 2007 10:32:57 PM2007-12-27Thursday, December 27, 2007 9:57:24 PM2007-12-28Friday, December 28, 2007 9:21:51 PM2007-12-29Saturday, December 29, 2007 8:46:18 PM2007-12-30Sunday, December 30, 2007 8:10:45 PM2007-12-31Monday, December 31, 2007 7:35:12 PM2008-01-01Tuesday, January 01, 2008 6:59:39 PM2008-01-02Wednesday, January 02, 2008 6:24:06 PM2008-01-03Thursday, January 03, 2008 5:48:33 PM2008-01-04Friday, January 04, 2008 5:13:00 PM2008-01-05Saturday, January 05, 2008 4:37:27 PM2008-01-06Sunday, January 06, 2008 4:01:54 PM2008-01-07Monday, January 07, 2008 3:26:21 PM2008-01-08Tuesday, January 08, 2008 2:50:48 PM2008-01-09Wednesday, January 09, 2008 2:15:15 PM2008-01-10Thursday, January 10, 2008 1:39:42 PM2008-01-11Friday, January 11, 2008 1:04:09 PM2008-01-12Saturday, January 12, 2008 12:28:36 PM2008-01-13Sunday, January 13, 2008 11:53:02 AM2008-01-14Monday, January 14, 2008 11:17:29 AM2008-01-15Tuesday, January 15, 2008 10:41:56 AM2008-01-16Wednesday, January 16, 2008 10:06:23 AM2008-01-17Thursday, January 17, 2008 9:30:50 AM2008-01-18Friday, January 18, 2008 8:55:17 AM2008-01-19Saturday, January 19, 2008 8:19:44 AM2008-01-20Sunday, January 20, 2008 7:44:11 AM2008-01-21Monday, January 21, 2008 7:08:38 AM2008-01-22Tuesday, January 22, 2008 6:33:05 AM2008-01-23Wednesday, January 23, 2008 5:57:32 AM2008-01-24Thursday, January 24, 2008 5:21:59 AM2008-01-25Friday, January 25, 2008 4:46:26 AM2008-01-26Saturday, January 26, 2008 4:10:53 AM2008-01-27Sunday, January 27, 2008 3:35:20 AM2008-01-28Monday, January 28, 2008 2:59:47 AM2008-01-29Tuesday, January 29, 2008 2:24:14 AM2008-01-30Wednesday, January 30, 2008 1:48:41 AM2008-01-31Thursday, January 31, 2008 1:13:08 AM2008-02-01Friday, February 01, 2008 0:37:35 AM2008-02-02Saturday, February 02, 2008 0:02:02 AM2008-02-02Saturday, February 02, 2008 11:26:29 PM2008-02-03Sunday, February 03, 2008 10:50:56 PM2008-02-04Monday, February 04, 2008 10:15:22 PM2008-02-05Tuesday, February 05, 2008 9:39:49 PM2008-02-06Wednesday, February 06, 2008 9:04:16 PM2008-02-07Thursday, February 07, 2008 8:28:43 PM2008-02-08Friday, February 08, 2008 7:53:10 PM2008-02-09Saturday, February 09, 2008 7:17:37 PM2008-02-10Sunday, February 10, 2008 6:42:04 PM2008-02-11Monday, February 11, 2008 6:06:31 PM2008-02-12Tuesday, February 12, 2008 5:30:58 PM2008-02-13Wednesday, February 13, 2008 4:55:25 PM2008-02-14Thursday, February 14, 2008 4:19:52 PM2008-02-15Friday, February 15, 2008 3:44:19 PM2008-02-16Saturday, February 16, 2008 3:08:46 PM2008-02-17Sunday, February 17, 2008 2:33:13 PM2008-02-18Monday, February 18, 2008 1:57:40 PM2008-02-19Tuesday, February 19, 2008 1:22:07 PM2008-02-20Wednesday, February 20, 2008 12:46:34 PM2008-02-21Thursday, February 21, 2008 12:11:01 PM2008-02-22Friday, February 22, 2008 11:35:28 AM2008-02-23Saturday, February 23, 2008 10:59:55 AM2008-02-24Sunday, February 24, 2008 10:24:22 AM2008-02-25Monday, February 25, 2008 9:48:49 AM2008-02-26Tuesday, February 26, 2008 9:13:15 AM2008-02-27Wednesday, February 27, 2008 8:37:42 AM2008-02-28Thursday, February 28, 2008 8:02:09 AM2008-02-29Friday, February 29, 2008 7:26:36 AM2008-03-01Saturday, March 01, 2008 6:51:03 AM2008-03-02Sunday, March 02, 2008 6:15:30 AM2008-03-03Monday, March 03, 2008 5:39:57 AM2008-03-04Tuesday, March 04, 2008 5:04:24 AM2008-03-05Wednesday, March 05, 2008 4:28:51 AM2008-03-06Thursday, March 06, 2008 3:53:18 AM2008-03-07Friday, March 07, 2008 3:17:45 AM2008-03-08Saturday, March 08, 2008 2:42:12 AM2008-03-09Sunday, March 09, 2008 3:06:39 AM2008-03-10Monday, March 10, 2008 2:31:06 AM2008-03-11Tuesday, March 11, 2008 1:55:33 AM2008-03-12Wednesday, March 12, 2008 1:20:00 AM2008-03-13Thursday, March 13, 2008 0:44:27 AM2008-03-14Friday, March 14, 2008 0:08:54 AM2008-03-14Friday, March 14, 2008 11:33:21 PM2008-03-15Saturday, March 15, 2008 10:57:48 PM2008-03-16Sunday, March 16, 2008 10:22:15 PM2008-03-17Monday, March 17, 2008 9:46:42 PM2008-03-18Tuesday, March 18, 2008 9:11:09 PM2008-03-19Wednesday, March 19, 2008 8:35:35 PM2008-03-20Thursday, March 20, 2008 8:00:02 PM2008-03-21Friday, March 21, 2008 7:24:29 PM2008-03-22Saturday, March 22, 2008 6:48:56 PM2008-03-23Sunday, March 23, 2008 6:13:23 PM2008-03-24Monday, March 24, 2008 5:37:50 PM2008-03-25Tuesday, March 25, 2008 5:02:17 PM2008-03-26Wednesday, March 26, 2008 4:26:44 PM2008-03-27Thursday, March 27, 2008 3:51:11 PM2008-03-28Friday, March 28, 2008 3:15:38 PM2008-03-29Saturday, March 29, 2008 2:40:05 PM2008-03-30Sunday, March 30, 2008 2:04:32 PM2008-03-31Monday, March 31, 2008 1:28:59 PM2008-04-01Tuesday, April 01, 2008 12:53:26 PM2008-04-02Wednesday, April 02, 2008 12:17:53 PM2008-04-03Thursday, April 03, 2008 11:42:20 AM2008-04-04Friday, April 04, 2008 11:06:47 AM2008-04-05Saturday, April 05, 2008 10:31:14 AM2008-04-06Sunday, April 06, 2008 9:55:41 AM2008-04-07Monday, April 07, 2008 9:20:08 AM2008-04-08Tuesday, April 08, 2008 8:44:35 AM2008-04-09Wednesday, April 09, 2008 8:09:02 AM2008-04-10Thursday, April 10, 2008 7:33:29 AM2008-04-11Friday, April 11, 2008 6:57:55 AM2008-04-12Saturday, April 12, 2008 6:22:22 AM2008-04-13Sunday, April 13, 2008 5:46:49 AM2008-04-14Monday, April 14, 2008 5:11:16 AM2008-04-15Tuesday, April 15, 2008 4:35:43 AM2008-04-16Wednesday, April 16, 2008 4:00:10 AM2008-04-17Thursday, April 17, 2008 3:24:37 AM2008-04-18Friday, April 18, 2008 2:49:04 AM2008-04-19Saturday, April 19, 2008 2:13:31 AM2008-04-20Sunday, April 20, 2008 1:37:58 AM2008-04-21Monday, April 21, 2008 1:02:25 AM2008-04-22Tuesday, April 22, 2008 0:26:52 AM2008-04-22Tuesday, April 22, 2008 11:51:19 PM2008-04-23Wednesday, April 23, 2008 11:15:46 PM2008-04-24Thursday, April 24, 2008 10:40:13 PM2008-04-25Friday, April 25, 2008 10:04:40 PM2008-04-26Saturday, April 26, 2008 9:29:07 PM2008-04-27Sunday, April 27, 2008 8:53:34 PM2008-04-28Monday, April 28, 2008 8:18:01 PM2008-04-29Tuesday, April 29, 2008 7:42:28 PM2008-04-30Wednesday, April 30, 2008 7:06:55 PM2008-05-01Thursday, May 01, 2008 6:31:22 PM"
assertEq(ret, expected);
