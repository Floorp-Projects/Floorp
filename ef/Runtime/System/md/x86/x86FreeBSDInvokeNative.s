/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 *
 * A per function stub should look like this (N is the number of incoming arguments)
 *
 * functionStub:	
 * 		push $function
 *		jmp sysInvokeNative##N		
 */		

#include <md/Asm.h>

.file "x86FreeBSDInvokeNative.S"
								
#define sysInvokeNative(N);											\
STATIC_ENTRY(CAT(sysInvokeNative,N));										\
		popl	%eax;						/* pop the function address */		\
														\
		pushl	%ebp;						/************************************/	\
		movl	%esp,%ebp;					/*				    */	\
		pushl	%edi;						/*		PROLOG		    */	\
		pushl	%esi;						/*			 	    */	\
		pushl	%ebx;						/************************************/	\
														\
		movl	$N,%edx;					/* %edx = nArgs                     */	\
														\
SYMBOL_NAME_LABEL(CAT(top,N));											\
														\
		leal	(16+4*N)(%esp),%ecx;				/* %ecx = %esp +  16 + 4 * nArgs    */	\
		pushl	(%ecx);						/* push original argument	    */	\
		decl	%edx;						/* nArgs -= 1			    */	\
		jne	SYMBOL_NAME(CAT(top,N));			/* loop over			    */	\
		call	*%eax;						/* call the native function	    */	\
														\
		movl	%ebp,%esp;					/************************************/	\
		popl	%ebp;						/*		EPILOG		    */	\
		ret		$(4*N);					/************************************/	\
END_ENTRY(CAT(sysInvokeNative,N))

TEXT_SEGMENT
		
STATIC_ENTRY(sysInvokeNative0)
		popl	%eax
		pushl	%ebp
		movl	%esp,%ebp
		pushl	%edi
		pushl	%esi
		pushl	%ebx
		call	*%eax
		movl	%ebp,%esp
		popl	%ebp
		ret
END_ENTRY(sysInvokeNative0)
				
STATIC_ENTRY(sysInvokeNative1)
		popl	%eax
		pushl	%ebp
		movl	%esp,%ebp
		pushl	%edi
		pushl	%esi
		pushl	%ebx
		movl	20(%esp),%ecx
		pushl	%ecx
		call	*%eax
		movl	%ebp,%esp
		popl	%ebp
		ret		$4
END_ENTRY(sysInvokeNative1)
		
STATIC_ENTRY(sysInvokeNative2)
		popl	%eax
		pushl	%ebp
		movl	%esp,%ebp
		pushl	%edi
		pushl	%esi
		pushl	%ebx
		movl	24(%esp),%ecx
		pushl	%ecx
		movl	24(%esp),%ecx
		pushl	%ecx
		call	*%eax
		movl	%ebp,%esp
		popl	%ebp
		ret		$8
END_ENTRY(sysInvokeNative2)

STATIC_ENTRY(sysInvokeNative3)
		popl	%eax
		pushl	%ebp
		movl	%esp,%ebp
		pushl	%edi
		pushl	%esi
		pushl	%ebx
		movl	28(%esp),%ecx
		pushl	%ecx
		movl	28(%esp),%ecx
		pushl	%ecx
		movl	28(%esp),%ecx
		pushl	%ecx
		call	*%eax
		movl	%ebp,%esp
		popl	%ebp
		ret		$12
END_ENTRY(sysInvokeNative3)

STATIC_ENTRY(sysInvokeNative4)
		popl	%eax
		pushl	%ebp
		movl	%esp,%ebp
		pushl	%edi
		pushl	%esi
		pushl	%ebx
		movl	32(%esp),%ecx
		pushl	%ecx
		movl	32(%esp),%ecx
		pushl	%ecx
		movl	32(%esp),%ecx
		pushl	%ecx
		movl	32(%esp),%ecx
		pushl	%ecx
		call	*%eax
		movl	%ebp,%esp
		popl	%ebp
		ret		$16
END_ENTRY(sysInvokeNative4)

STATIC_ENTRY(sysInvokeNative5)
		popl	%eax
		pushl	%ebp
		movl	%esp,%ebp
		pushl	%edi
		pushl	%esi
		pushl	%ebx
		movl	36(%esp),%ecx
		pushl	%ecx
		movl	36(%esp),%ecx
		pushl	%ecx
		movl	36(%esp),%ecx
		pushl	%ecx
		movl	36(%esp),%ecx
		pushl	%ecx
		movl	36(%esp),%ecx
		pushl	%ecx
		call	*%eax
		movl	%ebp,%esp
		popl	%ebp
		ret		$20
END_ENTRY(sysInvokeNative5)

sysInvokeNative(6);   sysInvokeNative(7);   sysInvokeNative(8);   sysInvokeNative(9);   sysInvokeNative(10); 
sysInvokeNative(11);  sysInvokeNative(12);  sysInvokeNative(13);  sysInvokeNative(14);  sysInvokeNative(15); 
sysInvokeNative(16);  sysInvokeNative(17);  sysInvokeNative(18);  sysInvokeNative(19);  sysInvokeNative(20); 
sysInvokeNative(21);  sysInvokeNative(22);  sysInvokeNative(23);  sysInvokeNative(24);  sysInvokeNative(25); 
sysInvokeNative(26);  sysInvokeNative(27);  sysInvokeNative(28);  sysInvokeNative(29);  sysInvokeNative(30); 
sysInvokeNative(31);  sysInvokeNative(32);  sysInvokeNative(33);  sysInvokeNative(34);  sysInvokeNative(35); 
sysInvokeNative(36);  sysInvokeNative(37);  sysInvokeNative(38);  sysInvokeNative(39);  sysInvokeNative(40); 
sysInvokeNative(41);  sysInvokeNative(42);  sysInvokeNative(43);  sysInvokeNative(44);  sysInvokeNative(45); 
sysInvokeNative(46);  sysInvokeNative(47);  sysInvokeNative(48);  sysInvokeNative(49);  sysInvokeNative(50); 
sysInvokeNative(51);  sysInvokeNative(52);  sysInvokeNative(53);  sysInvokeNative(54);  sysInvokeNative(55); 
sysInvokeNative(56);  sysInvokeNative(57);  sysInvokeNative(58);  sysInvokeNative(59);  sysInvokeNative(60); 
sysInvokeNative(61);  sysInvokeNative(62);  sysInvokeNative(63);  sysInvokeNative(64);  sysInvokeNative(65); 
sysInvokeNative(66);  sysInvokeNative(67);  sysInvokeNative(68);  sysInvokeNative(69);  sysInvokeNative(70); 
sysInvokeNative(71);  sysInvokeNative(72);  sysInvokeNative(73);  sysInvokeNative(74);  sysInvokeNative(75); 
sysInvokeNative(76);  sysInvokeNative(77);  sysInvokeNative(78);  sysInvokeNative(79);  sysInvokeNative(80); 
sysInvokeNative(81);  sysInvokeNative(82);  sysInvokeNative(83);  sysInvokeNative(84);  sysInvokeNative(85); 
sysInvokeNative(86);  sysInvokeNative(87);  sysInvokeNative(88);  sysInvokeNative(89);  sysInvokeNative(90); 
sysInvokeNative(91);  sysInvokeNative(92);  sysInvokeNative(93);  sysInvokeNative(94);  sysInvokeNative(95); 
sysInvokeNative(96);  sysInvokeNative(97);  sysInvokeNative(98);  sysInvokeNative(99);  sysInvokeNative(100); 
sysInvokeNative(101); sysInvokeNative(102); sysInvokeNative(103); sysInvokeNative(104); sysInvokeNative(105); 
sysInvokeNative(106); sysInvokeNative(107); sysInvokeNative(108); sysInvokeNative(109); sysInvokeNative(110); 
sysInvokeNative(111); sysInvokeNative(112); sysInvokeNative(113); sysInvokeNative(114); sysInvokeNative(115); 
sysInvokeNative(116); sysInvokeNative(117); sysInvokeNative(118); sysInvokeNative(119); sysInvokeNative(120); 
sysInvokeNative(121); sysInvokeNative(122); sysInvokeNative(123); sysInvokeNative(124); sysInvokeNative(125); 
sysInvokeNative(126); sysInvokeNative(127); sysInvokeNative(128); sysInvokeNative(129); sysInvokeNative(130); 
sysInvokeNative(131); sysInvokeNative(132); sysInvokeNative(133); sysInvokeNative(134); sysInvokeNative(135); 
sysInvokeNative(136); sysInvokeNative(137); sysInvokeNative(138); sysInvokeNative(139); sysInvokeNative(140); 
sysInvokeNative(141); sysInvokeNative(142); sysInvokeNative(143); sysInvokeNative(144); sysInvokeNative(145); 
sysInvokeNative(146); sysInvokeNative(147); sysInvokeNative(148); sysInvokeNative(149); sysInvokeNative(150); 
sysInvokeNative(151); sysInvokeNative(152); sysInvokeNative(153); sysInvokeNative(154); sysInvokeNative(155); 
sysInvokeNative(156); sysInvokeNative(157); sysInvokeNative(158); sysInvokeNative(159); sysInvokeNative(160); 
sysInvokeNative(161); sysInvokeNative(162); sysInvokeNative(163); sysInvokeNative(164); sysInvokeNative(165); 
sysInvokeNative(166); sysInvokeNative(167); sysInvokeNative(168); sysInvokeNative(169); sysInvokeNative(170); 
sysInvokeNative(171); sysInvokeNative(172); sysInvokeNative(173); sysInvokeNative(174); sysInvokeNative(175); 
sysInvokeNative(176); sysInvokeNative(177); sysInvokeNative(178); sysInvokeNative(179); sysInvokeNative(180); 
sysInvokeNative(181); sysInvokeNative(182); sysInvokeNative(183); sysInvokeNative(184); sysInvokeNative(185); 
sysInvokeNative(186); sysInvokeNative(187); sysInvokeNative(188); sysInvokeNative(189); sysInvokeNative(190); 
sysInvokeNative(191); sysInvokeNative(192); sysInvokeNative(193); sysInvokeNative(194); sysInvokeNative(195); 
sysInvokeNative(196); sysInvokeNative(197); sysInvokeNative(198); sysInvokeNative(199); sysInvokeNative(200); 
sysInvokeNative(201); sysInvokeNative(202); sysInvokeNative(203); sysInvokeNative(204); sysInvokeNative(205); 
sysInvokeNative(206); sysInvokeNative(207); sysInvokeNative(208); sysInvokeNative(209); sysInvokeNative(210); 
sysInvokeNative(211); sysInvokeNative(212); sysInvokeNative(213); sysInvokeNative(214); sysInvokeNative(215); 
sysInvokeNative(216); sysInvokeNative(217); sysInvokeNative(218); sysInvokeNative(219); sysInvokeNative(220); 
sysInvokeNative(221); sysInvokeNative(222); sysInvokeNative(223); sysInvokeNative(224); sysInvokeNative(225); 
sysInvokeNative(226); sysInvokeNative(227); sysInvokeNative(228); sysInvokeNative(229); sysInvokeNative(230); 
sysInvokeNative(231); sysInvokeNative(232); sysInvokeNative(233); sysInvokeNative(234); sysInvokeNative(235); 
sysInvokeNative(236); sysInvokeNative(237); sysInvokeNative(238); sysInvokeNative(239); sysInvokeNative(240); 
sysInvokeNative(241); sysInvokeNative(242); sysInvokeNative(243); sysInvokeNative(244); sysInvokeNative(245); 
sysInvokeNative(246); sysInvokeNative(247); sysInvokeNative(248); sysInvokeNative(249); sysInvokeNative(250); 
sysInvokeNative(251); sysInvokeNative(252); sysInvokeNative(253); sysInvokeNative(254); sysInvokeNative(255); 

DATA_SEGMENT

#define STUB(N) .long SYMBOL_NAME(CAT(sysInvokeNative,N))

GLOBAL_ENTRY(sysInvokeNativeStubs)
		STUB(0);    STUB(1);    STUB(2);    STUB(3);    STUB(4);    STUB(5);    STUB(6);    STUB(7); 
		STUB(8);    STUB(9);    STUB(10);   STUB(11);   STUB(12);   STUB(13);   STUB(14);   STUB(15); 
		STUB(16);   STUB(17);   STUB(18);   STUB(19);   STUB(20);   STUB(21);   STUB(22);   STUB(23); 
		STUB(24);   STUB(25);   STUB(26);   STUB(27);   STUB(28);   STUB(29);   STUB(30);   STUB(31); 
		STUB(32);   STUB(33);   STUB(34);   STUB(35);   STUB(36);   STUB(37);   STUB(38);   STUB(39); 
		STUB(40);   STUB(41);   STUB(42);   STUB(43);   STUB(44);   STUB(45);   STUB(46);   STUB(47); 
		STUB(48);   STUB(49);   STUB(50);   STUB(51);   STUB(52);   STUB(53);   STUB(54);   STUB(55); 
		STUB(56);   STUB(57);   STUB(58);   STUB(59);   STUB(60);   STUB(61);   STUB(62);   STUB(63); 
		STUB(64);   STUB(65);   STUB(66);   STUB(67);   STUB(68);   STUB(69);   STUB(70);   STUB(71); 
		STUB(72);   STUB(73);   STUB(74);   STUB(75);   STUB(76);   STUB(77);   STUB(78);   STUB(79); 
		STUB(80);   STUB(81);   STUB(82);   STUB(83);   STUB(84);   STUB(85);   STUB(86);   STUB(87); 
		STUB(88);   STUB(89);   STUB(90);   STUB(91);   STUB(92);   STUB(93);   STUB(94);   STUB(95); 
		STUB(96);   STUB(97);   STUB(98);   STUB(99);   STUB(100);  STUB(101);  STUB(102);  STUB(103); 
		STUB(104);  STUB(105);  STUB(106);  STUB(107);  STUB(108);  STUB(109);  STUB(110);  STUB(111); 
		STUB(112);  STUB(113);  STUB(114);  STUB(115);  STUB(116);  STUB(117);  STUB(118);  STUB(119); 
		STUB(120);  STUB(121);  STUB(122);  STUB(123);  STUB(124);  STUB(125);  STUB(126);  STUB(127); 
		STUB(128);  STUB(129);  STUB(130);  STUB(131);  STUB(132);  STUB(133);  STUB(134);  STUB(135);  
		STUB(136);  STUB(137);  STUB(138);  STUB(139);  STUB(140);  STUB(141);  STUB(142);  STUB(143); 
		STUB(144);  STUB(145);  STUB(146);  STUB(147);  STUB(148);  STUB(149);  STUB(150);  STUB(151); 
		STUB(152);  STUB(153);  STUB(154);  STUB(155);  STUB(156);  STUB(157);  STUB(158);  STUB(159); 
		STUB(160);  STUB(161);  STUB(162);  STUB(163);  STUB(164);  STUB(165);  STUB(166);  STUB(167); 
		STUB(168);  STUB(169);  STUB(170);  STUB(171);  STUB(172);  STUB(173);  STUB(174);  STUB(175); 
		STUB(176);  STUB(177);  STUB(178);  STUB(179);  STUB(180);  STUB(181);  STUB(182);  STUB(183); 
		STUB(184);  STUB(185);  STUB(186);  STUB(187);  STUB(188);  STUB(189);  STUB(190);  STUB(191); 
		STUB(192);  STUB(193);  STUB(194);  STUB(195);  STUB(196);  STUB(197);  STUB(198);  STUB(199);  
		STUB(200);  STUB(201);  STUB(202);  STUB(203);  STUB(204);  STUB(205);  STUB(206);  STUB(207); 
		STUB(208);  STUB(209);  STUB(210);  STUB(211);  STUB(212);  STUB(213);  STUB(214);  STUB(215); 
		STUB(216);  STUB(217);  STUB(218);  STUB(219);  STUB(220);  STUB(221);  STUB(222);  STUB(223); 
		STUB(224);  STUB(225);  STUB(226);  STUB(227);  STUB(228);  STUB(229);  STUB(230);  STUB(231); 
		STUB(232);  STUB(233);  STUB(234);  STUB(235);  STUB(236);  STUB(237);  STUB(238);  STUB(239); 
		STUB(240);  STUB(241);  STUB(242);  STUB(243);  STUB(244);  STUB(245);  STUB(246);  STUB(247); 
		STUB(248);  STUB(249);  STUB(250);  STUB(251);  STUB(252);  STUB(253);  STUB(254);  STUB(255); 
		.size sysInvokeNativeStubs,1024
