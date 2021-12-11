/* should not crash on SPARC */
var http_string = 'https://easylist-downloads.adblockplus.org/easylist.txt';
var regexp = /^[\w\-]+:\/+(?!\/)(?:[^.\/]+\.)*?reports\.adblockplus\.org(?:[\x00-\x24\x26-\x2C\x2F\x3A-\x40\x5B-\x5E\x60\x7B-\x80]|$)/i;
assertEq(http_string.match(regexp), null);
