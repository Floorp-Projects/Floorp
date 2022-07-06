/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RemoteAccessibleShared_h
#define mozilla_a11y_RemoteAccessibleShared_h

/**
 * These are function declarations shared between win/RemoteAccessible.h and
 * other/RemoteAccessible.h.
 */

/*
 * Return the states for the proxied accessible.
 */
virtual uint64_t State() override;

/*
 * Return the native states for the proxied accessible.
 */
uint64_t NativeState() const;

/*
 * Set aName to the name of the proxied accessible.
 * Return the ENameValueFlag passed from Accessible::Name
 */
ENameValueFlag Name(nsString& aName) const override;

/*
 * Set aValue to the value of the proxied accessible.
 */
void Value(nsString& aValue) const override;

/*
 * Set aHelp to the help string of the proxied accessible.
 */
void Help(nsString& aHelp) const;

/**
 * Set aDesc to the description of the proxied accessible.
 */
void Description(nsString& aDesc) const override;

/**
 * Get the set of attributes on the proxied accessible.
 */
virtual already_AddRefed<AccAttributes> Attributes() override;

/**
 * Return set of targets of given relation type.
 */
nsTArray<RemoteAccessible*> RelationByType(RelationType aType) const;

/**
 * Get all relations for this accessible.
 */
void Relations(nsTArray<RelationType>* aTypes,
               nsTArray<nsTArray<RemoteAccessible*>>* aTargetSets) const;

bool IsSearchbox() const;

nsStaticAtom* ARIARoleAtom() const;

virtual mozilla::a11y::GroupPos GroupPosition() override;
void ScrollToPoint(uint32_t aScrollType, int32_t aX, int32_t aY);

void Announce(const nsString& aAnnouncement, uint16_t aPriority);

int32_t CaretLineNumber();
virtual int32_t CaretOffset() const override;

virtual void TextSubstring(int32_t aStartOffset, int32_t aEndOfset,
                           nsAString& aText) const override;

virtual void TextAfterOffset(int32_t aOffset,
                             AccessibleTextBoundary aBoundaryType,
                             int32_t* aStartOffset, int32_t* aEndOffset,
                             nsAString& aText) override;

virtual void TextAtOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                          int32_t* aStartOffset, int32_t* aEndOffset,
                          nsAString& aText) override;

virtual void TextBeforeOffset(int32_t aOffset,
                              AccessibleTextBoundary aBoundaryType,
                              int32_t* aStartOffset, int32_t* aEndOffset,
                              nsAString& aText) override;

char16_t CharAt(int32_t aOffset);

virtual int32_t OffsetAtPoint(int32_t aX, int32_t aY,
                              uint32_t aCoordType) override;

bool SetSelectionBoundsAt(int32_t aSelectionNum, int32_t aStartOffset,
                          int32_t aEndOffset);

bool AddToSelection(int32_t aStartOffset, int32_t aEndOffset);

bool RemoveFromSelection(int32_t aSelectionNum);

void ScrollSubstringTo(int32_t aStartOffset, int32_t aEndOffset,
                       uint32_t aScrollType);

void ScrollSubstringToPoint(int32_t aStartOffset, int32_t aEndOffset,
                            uint32_t aCoordinateType, int32_t aX, int32_t aY);

void Text(nsString* aText);

void ReplaceText(const nsString& aText);

bool InsertText(const nsString& aText, int32_t aPosition);

bool CopyText(int32_t aStartPos, int32_t aEndPos);

bool CutText(int32_t aStartPos, int32_t aEndPos);

bool DeleteText(int32_t aStartPos, int32_t aEndPos);

bool PasteText(int32_t aPosition);

LayoutDeviceIntPoint ImagePosition(uint32_t aCoordType);

LayoutDeviceIntSize ImageSize();

bool IsLinkValid();

uint32_t AnchorCount(bool* aOk);

void AnchorURIAt(uint32_t aIndex, nsCString& aURI, bool* aOk);

RemoteAccessible* AnchorAt(uint32_t aIndex);

uint32_t LinkCount();

RemoteAccessible* LinkAt(const uint32_t& aIndex);

RemoteAccessible* TableOfACell();

uint32_t ColIdx();

uint32_t RowIdx();

void GetPosition(uint32_t* aColIdx, uint32_t* aRowIdx);

uint32_t ColExtent();

uint32_t RowExtent();

void GetColRowExtents(uint32_t* aColIdx, uint32_t* aRowIdx,
                      uint32_t* aColExtent, uint32_t* aRowExtent);

void ColHeaderCells(nsTArray<RemoteAccessible*>* aCells);

void RowHeaderCells(nsTArray<RemoteAccessible*>* aCells);

bool IsCellSelected();

RemoteAccessible* TableCaption();
void TableSummary(nsString& aSummary);
uint32_t TableColumnCount();
uint32_t TableRowCount();
RemoteAccessible* TableCellAt(uint32_t aRow, uint32_t aCol);
int32_t TableCellIndexAt(uint32_t aRow, uint32_t aCol);
int32_t TableColumnIndexAt(uint32_t aCellIndex);
int32_t TableRowIndexAt(uint32_t aCellIndex);
void TableRowAndColumnIndicesAt(uint32_t aCellIndex, int32_t* aRow,
                                int32_t* aCol);
uint32_t TableColumnExtentAt(uint32_t aRow, uint32_t aCol);
uint32_t TableRowExtentAt(uint32_t aRow, uint32_t aCol);
void TableColumnDescription(uint32_t aCol, nsString& aDescription);
void TableRowDescription(uint32_t aRow, nsString& aDescription);
bool TableColumnSelected(uint32_t aCol);
bool TableRowSelected(uint32_t aRow);
bool TableCellSelected(uint32_t aRow, uint32_t aCol);
uint32_t TableSelectedCellCount();
uint32_t TableSelectedColumnCount();
uint32_t TableSelectedRowCount();
void TableSelectedCells(nsTArray<RemoteAccessible*>* aCellIDs);
void TableSelectedCellIndices(nsTArray<uint32_t>* aCellIndices);
void TableSelectedColumnIndices(nsTArray<uint32_t>* aColumnIndices);
void TableSelectedRowIndices(nsTArray<uint32_t>* aRowIndices);
void TableSelectColumn(uint32_t aCol);
void TableSelectRow(uint32_t aRow);
void TableUnselectColumn(uint32_t aCol);
void TableUnselectRow(uint32_t aRow);
RemoteAccessible* AtkTableColumnHeader(int32_t aCol);
RemoteAccessible* AtkTableRowHeader(int32_t aRow);

KeyBinding AccessKey();
KeyBinding KeyboardShortcut();
void AtkKeyBinding(nsString& aBinding);

double CurValue() const override;
double MinValue() const override;
double MaxValue() const override;
double Step() const override;
bool SetCurValue(double aValue);

RemoteAccessible* FocusedChild();
Accessible* ChildAtPoint(
    int32_t aX, int32_t aY,
    LocalAccessible::EWhichChildAtPoint aWhichChild) override;
LayoutDeviceIntRect Bounds() const override;
virtual nsIntRect BoundsInCSSPixels() const override;

void Language(nsString& aLocale);
void DocType(nsString& aType);
void Title(nsString& aTitle);
void MimeType(nsString aMime);
void URLDocTypeMimeType(nsString& aURL, nsString& aDocType,
                        nsString& aMimeType);

void Extents(bool aNeedsScreenCoords, int32_t* aX, int32_t* aY, int32_t* aWidth,
             int32_t* aHeight);

virtual void DOMNodeID(nsString& aID) const override;

#endif
