function baz() {}
function bar() {}
function foo() {
  bar();
}
foo();

// For testing, we require the script to be parsed off-htread. To schedule a
// script off-thread, we require the script to be at least 5 KB. Thus, here is
// one comment which is used to trick this heuristics:
//
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWWX0Oxoc;,....            ....,;coxO0XWWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWX0kdlc;'..                              ..';:ldk0XWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWNKkoc,..                                              ..,cok0NWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWKko:'.                                                          .':okKWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWKkl,.                                                                    .,lkKWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMNOo;.                                                                            .;oONMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWXkl'.                                                                                   'lkXWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMNOl'                                                                                          'lONMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMWKo,                                                                                                ,oKWMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMNk:.                                                                                                    .:kNMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMXx,                                                                                                          ,xXMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMWXd'                                                                                                              'dXWMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMXd'                                                                                                                  'dXMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMNx'                                                                                                                      'xNMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMWO;                                                                                                                          ;OWMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMKl.                                                                                                                            .lXMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMWk,                                                                                                                                ,kWMMMMMMMMMMMMM
// MMMMMMMMMMMMXo.                                                                                                                                  .lXMMMMMMMMMMMM
// MMMMMMMMMMW0;                                      ......                                                                                          ;0WMMMMMMMMMM
// MMMMMMMMMWk'                                ..,:loxxkOOkxdo:,.                                                                                      'kWMMMMMMMMM
// MMMMMMMMWx.                             .':lxO000000000000000ko;.                                                                                    .xWMMMMMMMM
// MMMMMMMNd.                          .'cdk00000000000000000000000x;.                              ..',;ccloodddddddollc:;,'..                          .dNMMMMMMM
// MMMMMMNd.                          ,dO000000000000000000000000000Oo'                        ..;coxk00000000000000000000000Okdlc;'.                     .dNMMMMMM
// MMMMMNd.                          .cO000000000000000000000000000000k;                    .;lxO0000000000000000000000000000000000Okoc,.                  .dWMMMMM
// MMMMWx.                             .'cO0000000000000000000000000000Oc.               .;ok0000K00000000000000000000000000000000000000ko,                 .kWMMMM
// MMMMO'                                ,kK0000000000000000000000000000Oc.            .:x000000000000000OkdoollcccllodxkO0000K00000000000k'                 'OMMMM
// MMMK;                               .'lO000000000000000000000000000000Ol'...       'd000000000000Odl;'..             ..',:ldkO00000000KO,                  ;KMMM
// MMNo                              ,dkO0K00000000000000000000000000000000OOkxdoc;,,ck0000000000Oo;.                          .'ck000000KO;                   oNMM
// MMk.                             .o0000000000000000000000000000000000000000000000000000000000d,                               .o000000K0:                   .OMM
// MX:                              .o00000000000000000000000000000000000000000000000000000000Oc.                                 c000000K0l.                   :XM
// Wx.                              .c00000000000000000000000000000000000000000000000000000000x,                                  :OK000000o.                   .xW
// X:                                .:dxO00000000000000000000000000000000000000000000000000000Oo'                                ,k0000000d.                    :X
// O.                                   .:xOO0000000000000000000000000000000000000000000000000000Ol.                              .x000000Kd.                    .O
// o                                      ..'''''''',,:lx000000000000000000000000000000000000000000x,                             .lOOOOkkko.                     o
// ;                                                    .:x00000000000000000000000000000000000000000O:.                            .........                      ;
// .                                                      'd00000000000000000000000000000000000000000Ol.                                                          .
// .                                                       ,k000000000000000000000000000000000000000000o.                                                         .
//                                                         .o0000000000000000000000000000000000000000000d.
//                                                          c00000000000000000000000000000000000000000000o.
//                                                          ;OK0000000000000000000000000000000000000000000o.
//                                                          ,kK00000000000000000000000000000000000000000000xoc:,'..
//                                                          .x00000000000000000000000000000000000000000000000000Okxolc;,'..
//                                                          .d000000000000000K000000000000000000000000000000000000000000OOxdl:,..
// .                                                        .o00000000000000OO0000000000000000000000000000000000000000000000000Oxo:'.                             .
// .                                                        .l0000000000000Oc:k0000000000000000000000000000000000000000000000000000Oxl,.                          .
// ;                                                         c000000000000Kk, ,x000000000000000000000000000000xodkO00000000000000000000xc.                        ;
// o                                     ..''..              :000000000000Kx'  ,k0000000000000000000000000000Oc. ..';codkO000000000000000k:.                      o
// O.                                 .,lxO00Okxl:,.         :O000000000000d.   ;k0000000000000000000000000000l.        ..,cok0000000000000d'                    .O
// X:                                ,d000000000000kdc;.     :O000000000000l.   .c0000000000000000000000000000o.             .;oO00000000000k;                   :X
// Wx.                              'x00000000000000000Oxl;..:O00000000000O:     .x000000000000000000000000000d.                'oO00000K0000k,                 .kW
// MX:                              c0000000000000000000000Oxk00000000000Kk,      cO00000000000000000000000000d.                  ;k0000000000d.                :XM
// MMO.                             :O000000000000000000000000000000000000x.      cO00000000000000000000000000d.                   ,k000000000O:               .OMM
// MMNo                             .d000000000000000000000000000000000000l.     .d000000000000000000000000000o.                    cO000000000o.             .oNMM
// MMMK;                             'd00000000000000000000000000000000000c      ,k000000000000000000000000000l.                    'x000000000d.             ;KMMM
// MMMMO'                             .lO000000000000000000000000000000000kl;.  .o00000000000000000000000000K0c                     .d000000000d.            'OMMMM
// MMMMWx.                              ;x00000000000000000000000000000000000ko:lO000000000000000000000000000O;                     .x000000000d.           .xWMMMM
// MMMMMWd.                              .ck00000000000000000000000000000000000000000000000000000000000000000k,                     ;kK00000000l.          .dWMMMMM
// MMMMMMNd.                               .:x000000000000000000000000000000000000000000000000000000000000000d.                    .o000000000O;          .dNMMMMMM
// MMMMMMMNd.                                .;dO000000000000000000000000000000000000000000000000000000000000l.                   .o0000000000d.         .dNMMMMMMM
// MMMMMMMMWx.            .,,'..                'cx00000000000000000000000000000000000000000000000000000000KO:                  .;d0000000000x,         .xWMMMMMMMM
// MMMMMMMMMWk'           'd00Oxdl;'.             .,lx000000000000000000000000000000000000000000000000000000k'                .:dO0000000000x,         'kWMMMMMMMMM
// MMMMMMMMMMW0;           .:x000000kd:'.            .,lk000000000000000000000000000000000000000000000000000d.           ..,cdk0K000000000Oo.         :0WMMMMMMMMMM
// MMMMMMMMMMMMXo.           .,ok000000Odc.            ,x000000000000000000000000000000000000000000000000000d,...'',;:cldxO000000000000K0d;.        .oXMMMMMMMMMMMM
// MMMMMMMMMMMMMWk,             .:x0000000Oo;..     .,oO000000000000000000000000000000000000000000000000000000OOOO00000000000000000000Od;.         ,kWMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMXl.             .;dO0000000Oxdoooxk0000000000OxxO0000000000000000000000000000000000000000000000000000000000000000Oxc'          .lXMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMWO;               'lk00000000000000000000Oxc' ..:oxO0K000000000000000000000000000000000000000000000000000000Oko:'.           ;OWMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMNx,               .;ok00000000000000kdc'.       ..;cdxO00000000000000000000000000000000000000000000Okxdoc;'.             ,xNMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMXd'                .,:ldxkkkkxdl:,.                ..,:cldxkkkO000000000000Okkxdlc:;;;:::::;;;,,'...                 'dXMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMXd'                   ......                             ....'',,,,,,,,'....                                     'dXWMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMXx,.                                                                                                         ,xXMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMNk:.                                                                                                    .:kNMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMWKo,.                                                                                              .,oKWMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMNOl'                                                                                          'lONMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMXkl'.                                                                                  .'lkXWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMNOo;.                                                                            .;oONMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWKkl;.                                                                    .,lkKWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWKko:'.                                                          .':okKWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWNKkoc,..                                              ..,cok0NWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWX0kdlc;'..                              ..';:ldk0XWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMWWX0Oxoc;,....            ....,;coxO0XNWMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
