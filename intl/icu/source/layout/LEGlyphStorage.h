/*
 **********************************************************************
 *   Copyright (C) 1998-2014, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __LEGLYPHSTORAGE_H
#define __LEGLYPHSTORAGE_H

#include "LETypes.h"
#include "LEInsertionList.h"

/**
 * \file 
 * \brief C++ API: This class encapsulates the per-glyph storage used by the ICU LayoutEngine.
 */

U_NAMESPACE_BEGIN

/**
 * This class encapsulates the per-glyph storage used by the ICU LayoutEngine.
 * For each glyph it holds the glyph ID, the index of the backing store character
 * which produced the glyph, the X and Y position of the glyph and an auxillary data
 * pointer.
 *
 * The storage is growable using the <code>LEInsertionList</code> class.
 *
 *
 * @see LEInsertionList.h
 *
 * @deprecated ICU 54. See {@link icu::LayoutEngine}
 */
class U_LAYOUT_API LEGlyphStorage : public UObject, protected LEInsertionCallback
{
private:
    /**
     * The number of entries in the per-glyph arrays.
     *
     * @internal
     */
    le_int32   fGlyphCount;

    /**
     * The glyph ID array.
     *
     * @internal
     */
    LEGlyphID *fGlyphs;
 
    /**
     * The char indices array.
     *
     * @internal
     */
    le_int32  *fCharIndices;

    /**
     * The glyph positions array.
     *
     * @internal
     */
    float     *fPositions;

    /**
     * The auxillary data array.
     *
     * @internal
     */
    le_uint32 *fAuxData;


    /**
     * The insertion list, used to grow the above arrays.
     *
     * @internal
     */
    LEInsertionList *fInsertionList;

    /**
     * The source index while growing the data arrays.
     *
     * @internal
     */
    le_int32 fSrcIndex;

    /**
     * The destination index used while growing the data arrays.
     *
     * @internal
     */
    le_int32 fDestIndex;

protected:
    /**
     * This implements <code>LEInsertionCallback</code>. The <code>LEInsertionList</code>
     * will call this method once for each insertion.
     *
     * @param atPosition the position of the insertion
     * @param count the number of glyphs being inserted
     * @param newGlyphs the address of the new glyph IDs
     *
     * @return <code>true</code> if <code>LEInsertionList</code> should stop
     *         processing the insertion list after this insertion.
     *
     * @see LEInsertionList.h
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    virtual le_bool applyInsertion(le_int32 atPosition, le_int32 count, LEGlyphID newGlyphs[]);

public:

    /**
     * Allocates an empty <code>LEGlyphStorage</code> object. You must call
     * <code>allocateGlyphArray, allocatePositions and allocateAuxData</code>
     * to allocate the data.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    LEGlyphStorage();

    /**
     * The destructor. This will deallocate all of the arrays.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    ~LEGlyphStorage();

    /**
     * This method returns the number of glyphs in the glyph array.
     *
     * @return the number of glyphs in the glyph array
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    inline le_int32 getGlyphCount() const;

    /**
     * This method copies the glyph array into a caller supplied array.
     * The caller must ensure that the array is large enough to hold all
     * the glyphs.
     *
     * @param glyphs - the destiniation glyph array
     * @param success - set to an error code if the operation fails
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void getGlyphs(LEGlyphID glyphs[], LEErrorCode &success) const;

    /**
     * This method copies the glyph array into a caller supplied array,
     * ORing in extra bits. (This functionality is needed by the JDK,
     * which uses 32 bits pre glyph idex, with the high 16 bits encoding
     * the composite font slot number)
     *
     * @param glyphs - the destination (32 bit) glyph array
     * @param extraBits - this value will be ORed with each glyph index
     * @param success - set to an error code if the operation fails
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void getGlyphs(le_uint32 glyphs[], le_uint32 extraBits, LEErrorCode &success) const;

    /**
     * This method copies the character index array into a caller supplied array.
     * The caller must ensure that the array is large enough to hold a
     * character index for each glyph.
     *
     * @param charIndices - the destiniation character index array
     * @param success - set to an error code if the operation fails
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void getCharIndices(le_int32 charIndices[], LEErrorCode &success) const;

    /**
     * This method copies the character index array into a caller supplied array.
     * The caller must ensure that the array is large enough to hold a
     * character index for each glyph.
     *
     * @param charIndices - the destiniation character index array
     * @param indexBase - an offset which will be added to each index
     * @param success - set to an error code if the operation fails
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void getCharIndices(le_int32 charIndices[], le_int32 indexBase, LEErrorCode &success) const;

    /**
     * This method copies the position array into a caller supplied array.
     * The caller must ensure that the array is large enough to hold an
     * X and Y position for each glyph, plus an extra X and Y for the
     * advance of the last glyph.
     *
     * @param positions - the destiniation position array
     * @param success - set to an error code if the operation fails
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void getGlyphPositions(float positions[], LEErrorCode &success) const;

    /**
     * This method returns the X and Y position of the glyph at
     * the given index.
     *
     * Input parameters:
     * @param glyphIndex - the index of the glyph
     *
     * Output parameters:
     * @param x - the glyph's X position
     * @param y - the glyph's Y position
     * @param success - set to an error code if the operation fails
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void getGlyphPosition(le_int32 glyphIndex, float &x, float &y, LEErrorCode &success) const;

    /**
     * This method allocates the glyph array, the char indices array and the insertion list. You
     * must call this method before using the object. This method also initializes the char indices
     * array.
     *
     * @param initialGlyphCount the initial size of the glyph and char indices arrays.
     * @param rightToLeft <code>true</code> if the original input text is right to left.
     * @param success set to an error code if the storage cannot be allocated of if the initial
     *        glyph count is not positive.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void allocateGlyphArray(le_int32 initialGlyphCount, le_bool rightToLeft, LEErrorCode &success);

    /**
     * This method allocates the storage for the glyph positions. It allocates one extra X, Y
     * position pair for the position just after the last glyph.
     *
     * @param success set to an error code if the positions array cannot be allocated.
     *
     * @return the number of X, Y position pairs allocated.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    le_int32 allocatePositions(LEErrorCode &success);

    /**
     * This method allocates the storage for the auxillary glyph data.
     *
     * @param success set to an error code if the aulillary data array cannot be allocated.
     *
     * @return the size of the auxillary data array.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    le_int32 allocateAuxData(LEErrorCode &success);

    /**
     * Copy the entire auxillary data array.
     *
     * @param auxData the auxillary data array will be copied to this address
     * @param success set to an error code if the data cannot be copied
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void getAuxData(le_uint32 auxData[], LEErrorCode &success) const;

    /**
     * Get the glyph ID for a particular glyph.
     *
     * @param glyphIndex the index into the glyph array
     * @param success set to an error code if the glyph ID cannot be retrieved.
     *
     * @return the glyph ID
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    LEGlyphID getGlyphID(le_int32 glyphIndex, LEErrorCode &success) const;

    /**
     * Get the char index for a particular glyph.
     *
     * @param glyphIndex the index into the glyph array
     * @param success set to an error code if the char index cannot be retrieved.
     *
     * @return the character index
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    le_int32  getCharIndex(le_int32 glyphIndex, LEErrorCode &success) const;


    /**
     * Get the auxillary data for a particular glyph.
     *
     * @param glyphIndex the index into the glyph array
     * @param success set to an error code if the auxillary data cannot be retrieved.
     *
     * @return the auxillary data
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    le_uint32 getAuxData(le_int32 glyphIndex, LEErrorCode &success) const;

    /**
     * This operator allows direct access to the glyph array
     * using the index operator.
     *
     * @param glyphIndex the index into the glyph array
     *
     * @return a reference to the given location in the glyph array
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    inline LEGlyphID &operator[](le_int32 glyphIndex) const;

    /**
     * Call this method to replace a single glyph in the glyph array
     * with multiple glyphs. This method uses the <code>LEInsertionList</code>
     * to do the insertion. It returns the address of storage where the new
     * glyph IDs can be stored. They will not actually be inserted into the
     * glyph array until <code>applyInsertions</code> is called.
     *
     * @param atIndex the index of the glyph to be replaced
     * @param insertCount the number of glyphs to replace it with
     * @param success set to an error code if the auxillary data cannot be retrieved.
     *
     * @return the address at which to store the replacement glyphs.
     *
     * @see LEInsertionList.h
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    LEGlyphID *insertGlyphs(le_int32 atIndex, le_int32 insertCount, LEErrorCode& success);

    /**
     * Call this method to replace a single glyph in the glyph array
     * with multiple glyphs. This method uses the <code>LEInsertionList</code>
     * to do the insertion. It returns the address of storage where the new
     * glyph IDs can be stored. They will not actually be inserted into the
     * glyph array until <code>applyInsertions</code> is called.
     *
     * Note: Don't use this version, use the other version of this function which has an error code.
     *
     * @param atIndex the index of the glyph to be replaced
     * @param insertCount the number of glyphs to replace it with
     *
     * @return the address at which to store the replacement glyphs.
     *
     * @see LEInsertionList.h
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    LEGlyphID *insertGlyphs(le_int32 atIndex, le_int32 insertCount);

    /**
     * This method is used to reposition glyphs during Indic v2 processing.  It moves 
     * all of the relevant glyph information ( glyph, indices, positions, and auxData ), 
     * from the source position to the target position, and also allows for a marker bit
     * to be set in the target glyph's auxData so that it won't be reprocessed later in the
     * cycle.
     *
     * @param fromPosition - position of the glyph to be moved
     * @param toPosition - target position of the glyph
     * @param marker marker bit
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void moveGlyph(le_int32 fromPosition, le_int32 toPosition, le_uint32 marker);

    /**
     * This method causes all of the glyph insertions recorded by
     * <code>insertGlyphs</code> to be applied to the glyph array. The
     * new slots in the char indices and the auxillary data arrays
     * will be filled in with the values for the glyph being replaced.
     *
     * @return the new size of the glyph array
     *
     * @see LEInsertionList.h
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    le_int32 applyInsertions();

    /**
     * Set the glyph ID for a particular glyph.
     *
     * @param glyphIndex the index of the glyph
     * @param glyphID the new glyph ID
     * @param success will be set to an error code if the glyph ID cannot be set.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void setGlyphID(le_int32 glyphIndex, LEGlyphID glyphID, LEErrorCode &success);

    /**
     * Set the char index for a particular glyph.
     *
     * @param glyphIndex the index of the glyph
     * @param charIndex the new char index
     * @param success will be set to an error code if the char index cannot be set.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void setCharIndex(le_int32 glyphIndex, le_int32 charIndex, LEErrorCode &success);

    /**
     * Set the X, Y position for a particular glyph.
     *
     * @param glyphIndex the index of the glyph
     * @param x the new X position
     * @param y the new Y position
     * @param success will be set to an error code if the position cannot be set.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void setPosition(le_int32 glyphIndex, float x, float y, LEErrorCode &success);

    /**
     * Adjust the X, Y position for a particular glyph.
     *
     * @param glyphIndex the index of the glyph
     * @param xAdjust the adjustment to the glyph's X position
     * @param yAdjust the adjustment to the glyph's Y position
     * @param success will be set to an error code if the glyph's position cannot be adjusted.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void adjustPosition(le_int32 glyphIndex, float xAdjust, float yAdjust, LEErrorCode &success);

    /**
     * Set the auxillary data for a particular glyph.
     *
     * @param glyphIndex the index of the glyph
     * @param auxData the new auxillary data
     * @param success will be set to an error code if the auxillary data cannot be set.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void setAuxData(le_int32 glyphIndex, le_uint32 auxData, LEErrorCode &success);

    /**
     * Delete the glyph array and replace it with the one
     * in <code>from</code>. Set the glyph array pointer
     * in <code>from</code> to <code>NULL</code>.
     *
     * @param from the <code>LEGlyphStorage</code> object from which
     *             to get the new glyph array.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void adoptGlyphArray(LEGlyphStorage &from);

    /**
     * Delete the char indices array and replace it with the one
     * in <code>from</code>. Set the char indices array pointer
     * in <code>from</code> to <code>NULL</code>.
     *
     * @param from the <code>LEGlyphStorage</code> object from which
     *             to get the new char indices array.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void adoptCharIndicesArray(LEGlyphStorage &from);

    /**
     * Delete the position array and replace it with the one
     * in <code>from</code>. Set the position array pointer
     * in <code>from</code> to <code>NULL</code>.
     *
     * @param from the <code>LEGlyphStorage</code> object from which
     *             to get the new position array.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void adoptPositionArray(LEGlyphStorage &from);

    /**
     * Delete the auxillary data array and replace it with the one
     * in <code>from</code>. Set the auxillary data array pointer
     * in <code>from</code> to <code>NULL</code>.
     *
     * @param from the <code>LEGlyphStorage</code> object from which
     *             to get the new auxillary data array.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void adoptAuxDataArray(LEGlyphStorage &from);

    /**
     * Change the glyph count of this object to be the same
     * as the one in <code>from</code>.
     *
     * @param from the <code>LEGlyphStorage</code> object from which
     *             to get the new glyph count.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void adoptGlyphCount(LEGlyphStorage &from);

    /**
     * Change the glyph count of this object to the given value.
     *
     * @param newGlyphCount the new glyph count.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void adoptGlyphCount(le_int32 newGlyphCount);

    /**
     * This method frees the glyph, character index, position  and
     * auxillary data arrays so that the LayoutEngine can be reused
     * to layout a different characer array. (This method is also called
     * by the destructor)
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    void reset();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    static UClassID getStaticClassID();
};

inline le_int32 LEGlyphStorage::getGlyphCount() const
{
    return fGlyphCount;
}

inline LEGlyphID &LEGlyphStorage::operator[](le_int32 glyphIndex) const
{
    return fGlyphs[glyphIndex];
}


U_NAMESPACE_END
#endif

