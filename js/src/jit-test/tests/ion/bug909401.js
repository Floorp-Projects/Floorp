var TZ_PST = -8;
var TZ_DIFF = GetTimezoneOffset();
var PST_DIFF = TZ_DIFF - TZ_PST;
function GetTimezoneOffset() {}
function adjustResultArray(ResultArray) {
    var t = ResultArray[TIME] - PST_DIFF;
    ResultArray[UTC_YEAR] = YearFromTime(t);
}
function TimeInYear( y ) {}
function YearFromTime( t ) {
    var sign = ( t < 0 ) ? -1 : 1;
    var year = ( sign < 0 ) ? 1969 : 1970;
    for ( var timeToTimeZero = t; ;  ) {
	timeToTimeZero -= sign * TimeInYear(year)
        break;
    }
    return ( year );
}
gczeal(4);
evaluate("\
var TIME        = 0;\
var UTC_YEAR    = 1;\
adjustResultArray([]);\
adjustResultArray([946684800000-1]);\
adjustResultArray([]);\
", { noScriptRval : true });
