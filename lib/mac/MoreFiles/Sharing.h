/*
**	Apple Macintosh Developer Technical Support
**
**	PBShare, PBUnshare, and PBGetUGEnty (should be in Files.h -
**	if not, include this file.)
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		Sharing.h
**
**	Copyright © 1992-1996 Apple Computer, Inc.
**	All rights reserved.
**
**	You may incorporate this sample code into your applications without
**	restriction, though the sample code has been provided "AS IS" and the
**	responsibility for its operation is 100% yours.  However, what you are
**	not permitted to do is to redistribute the source as "DSC Sample Code"
**	after having made changes. If you're going to re-distribute the source,
**	we require that you make it clear in the source that the code was
**	descended from Apple Sample Code, but that you've made changes.
*/

#ifndef __SHARING__
#define __SHARING__

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif

pascal OSErr PBShare(HParmBlkPtr paramBlock,
					 Boolean async) = {
								0x101F,		/* MOVE.B     (A7)+,D0   */
								0x205F,		/* MOVEA.L    (A7)+,A0   */
								0x6606,		/* BNE.S      *+$0008    */
								0x7042,		/* MOVEQ      #$42,D0    */
								0xA260,		/* _FSDispatch,Immed     */
								0x6004,		/* BRA.S      *+$0006    */
								0x7042,		/* MOVEQ      #$42,D0    */
								0xA660,		/* _FSDispatch,Sys,Immed */
								0x3E80};	/* MOVE.W     D0,(A7)    */

#pragma parameter __D0 PBShareSync(__A0)
pascal OSErr PBShareSync(HParmBlkPtr paramBlock) = {
								0x7042,		/* MOVEQ      #$42,D0    */
								0xA260};	/* _FSDispatch,Immed     */

#pragma parameter __D0 PBShareAsync(__A0)
pascal OSErr PBShareAsync(HParmBlkPtr paramBlock) = {
								0x7042,		/* MOVEQ      #$42,D0    */
								0xA660};	/* _FSDispatch,Sys,Immed */


pascal OSErr PBUnshare(HParmBlkPtr paramBlock,
					   Boolean async) = {
								0x101F,		/* MOVE.B     (A7)+,D0   */
								0x205F,		/* MOVEA.L    (A7)+,A0   */
								0x6606,		/* BNE.S      *+$0008    */
								0x7043,		/* MOVEQ      #$43,D0    */
								0xA260,		/* _FSDispatch,Immed     */
								0x6004,		/* BRA.S      *+$0006    */
								0x7043,		/* MOVEQ      #$43,D0    */
								0xA660,		/* _FSDispatch,Sys,Immed */
								0x3E80};	/* MOVE.W     D0,(A7)    */

#pragma parameter __D0 PBUnshareSync(__A0)
pascal OSErr PBUnshareSync(HParmBlkPtr paramBlock) = {
								0x7043,		/* MOVEQ      #$43,D0    */
								0xA260};	/* _FSDispatch,Immed     */

#pragma parameter __D0 PBUnshareAsync(__A0)
pascal OSErr PBUnshareAsync(HParmBlkPtr paramBlock) = {
								0x7043,		/* MOVEQ      #$43,D0    */
								0xA660};	/* _FSDispatch,Sys,Immed */


pascal OSErr PBGetUGEntry(HParmBlkPtr paramBlock,
						  Boolean async) = {
								0x101F,		/* MOVE.B     (A7)+,D0   */
								0x205F,		/* MOVEA.L    (A7)+,A0   */
								0x6606,		/* BNE.S      *+$0008    */
								0x7044,		/* MOVEQ      #$44,D0    */
								0xA260,		/* _FSDispatch,Immed     */
								0x6004,		/* BRA.S      *+$0006    */
								0x7044,		/* MOVEQ      #$44,D0    */
								0xA660,		/* _FSDispatch,Sys,Immed */
								0x3E80};	/* MOVE.W     D0,(A7)    */

#pragma parameter __D0 PBGetUGEntrySync(__A0)
pascal OSErr PBGetUGEntrySync(HParmBlkPtr paramBlock) = {
								0x7044,		/* MOVEQ      #$44,D0    */
								0xA260};	/* _FSDispatch,Immed     */

#pragma parameter __D0 PBGetUGEntryAsync(__A0)
pascal OSErr PBGetUGEntryAsync(HParmBlkPtr paramBlock) = {
								0x7044,		/* MOVEQ      #$44,D0    */
								0xA660};	/* _FSDispatch,Sys,Immed */

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __SHARING__ */
