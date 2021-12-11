// |reftest| skip-if(!this.hasOwnProperty("Intl"))
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Tests the format function with a diverse set of locales and options.
// Always use UTC to avoid dependencies on test environment.

const {
  Era, Year, Month, Weekday, Day, DayPeriod, Hour, Minute, Second, Literal
} = DateTimeFormatParts

var format;
var date = Date.UTC(2012, 11, 17, 3, 0, 42);

// Locale en-US; default options.
format = new Intl.DateTimeFormat("en-us", {timeZone: "UTC"});
assertParts(format, date, [
  Month("12"), Literal("/"), Day("17"), Literal("/"), Year("2012"),
]);

// Just date
format = new Intl.DateTimeFormat("en-us", {
  year: 'numeric',
  month: 'numeric',
  day: 'numeric',
  timeZone: "UTC"});
assertParts(format, date, [
  Month("12"), Literal("/"), Day("17"), Literal("/"), Year("2012"),
]);

// Just time in hour24
format = new Intl.DateTimeFormat("en-us", {
  hour: 'numeric',
  minute: 'numeric',
  second: 'numeric',
  hour12: false,
  timeZone: "UTC"});
assertParts(format, date, [
  Hour("03"), Literal(":"), Minute("00"), Literal(":"), Second("42"),
]);

// Just time in hour12
format = new Intl.DateTimeFormat("en-us", {
  hour: 'numeric',
  minute: 'numeric',
  second: 'numeric',
  hour12: true,
  timeZone: "UTC"});
assertParts(format, date, [
  Hour("3"), Literal(":"), Minute("00"), Literal(":"), Second("42"), Literal(" "), DayPeriod("AM"),
]);

// Just month.
format = new Intl.DateTimeFormat("en-us", {
  month: "narrow",
  timeZone: "UTC"});
assertParts(format, date, [
  Month("D"),
]);

// Just weekday.
format = new Intl.DateTimeFormat("en-us", {
  weekday: "narrow",
  timeZone: "UTC"});
assertParts(format, date, [
  Weekday("M"),
]);

// Year and era.
format = new Intl.DateTimeFormat("en-us", {
  year: "numeric",
  era: "short",
  timeZone: "UTC"});
assertParts(format, date, [
  Year("2012"), Literal(" "), Era("AD"),
]);

// Time and date
format = new Intl.DateTimeFormat("en-us", {
  weekday: 'long',
  year: 'numeric',
  month: 'numeric',
  day: 'numeric',
  hour: 'numeric',
  minute: 'numeric',
  second: 'numeric',
  hour12: true,
  timeZone: "UTC"});
assertParts(format, date, [
  Weekday("Monday"), Literal(", "), Month("12"), Literal("/"), Day("17"), Literal("/"), Year("2012"),
  Literal(", "),
  Hour("3"), Literal(":"), Minute("00"), Literal(":"), Second("42"), Literal(" "), DayPeriod("AM"),
]);

if (typeof reportCompare === "function")
    reportCompare(0, 0, 'ok');
