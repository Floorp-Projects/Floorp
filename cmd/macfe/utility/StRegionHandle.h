//////////////////////////////////

//
// StRegionHandle.h
//
//	by Drew Thaler, athaler@umich.edu
//	released 9/21/96
//
//	This source code is public domain.
//

//////////////////////////////////

// MGY: renamed to StRegionHandle (from StRegion) to avoid conflict with
// existing StRegion.

#ifndef _StRegionHandle_H_
#define _StRegionHandle_H_


#pragma mark -- StRegionHandle --

// -------------------------------------------------------------------------
//	¥ StRegionHandle
// -------------------------------------------------------------------------
//
//	This class is an abstraction of the Macintosh Region data structure,
//	implemented entirely with inline functions.  It's meant to provide an
//	easy-to-use interface to most of the standard region operations -- copying,
//	adding, subtracting, xor'ing, intersecting, converting to/from rectangles,
//	etc -- while using the magic of C++ to protect the user from the gory details.
//	
//	Benefits:	¥ Automatic memory management -- say goodbye to NewRgn, DisposeRgn, etc!
//				¥ Standard operators like +=, -=, &=, ==, etc for both Rects and RgnHandles
//				¥ Automatic coercion operators, can use directly in Toolbox calls
//				¥ No extra function calls: feels like C++, generates code like C.
//


class StRegionHandle
{

public:
	StRegionHandle()							{ mRegion = ::NewRgn(); }
	StRegionHandle(const RgnHandle fromRgn)	{ mRegion = ::NewRgn(); ::CopyRgn( fromRgn, mRegion ); }
	StRegionHandle(const Rect &fromRect)		{ mRegion = ::NewRgn(); ::RectRgn( mRegion, &fromRect ); }
	~StRegionHandle()							{ if (mRegion) ::DisposeRgn(mRegion); }
	
	Rect		bbox() const					{ return (**mRegion).rgnBBox; }
	void		GetBounds(Rect &outRect) const	{ outRect = (**mRegion).rgnBBox; }
	void		Clear()							{ ::SetEmptyRgn(mRegion); }
	Boolean		IsEmpty() const					{ return ::EmptyRgn(mRegion); }
	RgnHandle	Detach()						{ RgnHandle oldRegion = mRegion; mRegion = ::NewRgn(); return oldRegion; }
	
	Boolean		IsValid()						{ return (mRegion!=nil); }
	
	operator RgnHandle() const					{ return mRegion; }
	operator Handle() const						{ return (Handle)mRegion; }
	
	RgnHandle &	operator += ( RgnHandle r )		{ ::UnionRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle &	operator -= ( RgnHandle r )		{ ::DiffRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle &	operator |= ( RgnHandle r )		{ ::UnionRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle &	operator &= ( RgnHandle r )		{ ::SectRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle &	operator ^= ( RgnHandle r )		{ ::XorRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle & operator = ( RgnHandle r )		{ ::CopyRgn(r,mRegion); return mRegion; }
	
	RgnHandle & operator += ( Rect & rect )		{ StRegionHandle r(rect); ::UnionRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle & operator -= ( Rect & rect )		{ StRegionHandle r(rect); ::DiffRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle & operator |= ( Rect & rect )		{ StRegionHandle r(rect); ::UnionRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle & operator &= ( Rect & rect )		{ StRegionHandle r(rect); ::SectRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle & operator ^= ( Rect & rect )		{ StRegionHandle r(rect); ::XorRgn(mRegion,r,mRegion); return mRegion; }
	RgnHandle & operator = ( Rect &r )			{ ::RectRgn(mRegion,&r); return mRegion; }
	
	Boolean operator == ( RgnHandle r )			{ return ::EqualRgn(r,mRegion); }
	Boolean operator != ( RgnHandle r )			{ return !::EqualRgn(r,mRegion); }
	
	Boolean operator == ( Rect & rect )			{ StRegionHandle r(rect); return ::EqualRgn(r,mRegion); }
	Boolean operator != ( Rect & rect )			{ StRegionHandle r(rect); return !::EqualRgn(r,mRegion); }
	
	// why not, so you can do "if rgn == 0", right?
	RgnHandle & operator = ( int x )			{ ::SetEmptyRgn(mRegion); return mRegion; }
	Boolean operator == ( int x )				{ return ( (x==0) && ::EmptyRgn(mRegion) ); }
	Boolean operator != ( int x )				{ return ( (x!=0) || !::EmptyRgn(mRegion) ); }
	
	
private:
	RgnHandle	mRegion;	
};


//////////////////////////////////


#pragma mark -- StTempRegion --

typedef class StRegionHandle	StTempRegion;






#endif // _StRegionHandle_H_

