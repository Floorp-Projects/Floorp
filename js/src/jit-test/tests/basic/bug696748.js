try {
this.watch("b", "".substring);
} catch(exc1) {}
eval("\
var URI = '';\
test();\
function test() {\
  URI += '<zcti application=\"xxxx_demo\">';\
  URI += '<pstn_data>';\
  URI += '<dnis>877-485-xxxx</dnis>';\
  URI += '</pstn_data>';\
  URI >>=  '<keyvalue key=\"name\" value=\"xxx\"/>';\
  URI += '<keyvalue key=\"phone\" value=\"6509309000\"/>';\
}\
test();\
");
