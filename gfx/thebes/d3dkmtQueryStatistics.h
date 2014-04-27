/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* This file is based on a header file that was briefly seen in the
 * Windows 8 RC SDK. The work for this file itself was based on the one in ProcessHacker at
 * http://processhacker.svn.sourceforge.net/viewvc/processhacker/2.x/trunk/plugins/ExtendedTools/d3dkmt.h?revision=4758&view=markup
 * For more details see Mozilla Bug 689870.
 * [Bug 917496 indicates that some of these structs may not match reality, and
 * therefore should not be trusted.  See the reference to bug 917496 in
 * gfxWindowsPlatform.cpp.]
 */

typedef struct _D3DKMTQS_COUNTER
{
    ULONG Count;
    ULONGLONG Bytes;
} D3DKMTQS_COUNTER;

typedef struct _D3DKMTQS_ADAPTER_INFO
{
    ULONG NbSegments;

    ULONG Filler[4];
    ULONGLONG Filler2[2]; // Assumed sizeof(LONGLONG) = sizeof(ULONGLONG)
    struct {
        ULONG Filler[14];
    } Filler_RDMAB;
    struct {
        ULONG Filler[9];
    } Filler_R;
    struct {
        ULONG Filler[4];
        D3DKMTQS_COUNTER Filler2;
    } Filler_P;
    struct {
        D3DKMTQS_COUNTER Filler[16];
        ULONG Filler2[2];
    } Filler_PF;
    struct {
        ULONGLONG Filler[8];
    } Filler_PT;
    struct {
        ULONG Filler[2];
    } Filler_SR;
    struct {
        ULONG Filler[7];
    } Filler_L;
    struct {
        D3DKMTQS_COUNTER Filler[7];
    } Filler_A;
    struct {
        D3DKMTQS_COUNTER Filler[4];
    } Filler_T;
    ULONG64 Reserved[8];
} D3DKMTQS_ADAPTER_INFO;

typedef struct _D3DKMTQS_SEGMENT_INFO_WIN7
{
    ULONG Filler[3];
    struct {
        ULONGLONG Filler;
        ULONG Filler2[2];
    } Filler_M;

    ULONG Aperture;

    ULONGLONG Filler3[5];
    ULONG64 Filler4[8];
} D3DKMTQS_SEGMENT_INFO_WIN7;

typedef struct _D3DKMTQS_SEGMENT_INFO_WIN8
{
    ULONGLONG Filler[3];
    struct {
        ULONGLONG Filler;
        ULONG Filler2[2];
    } Filler_M;

    ULONG Aperture;

    ULONGLONG Filler3[5];
    ULONG64 Filler4[8];
} D3DKMTQS_SEGMENT_INFO_WIN8;

typedef struct _D3DKMTQS_SYSTEM_MEMORY
{
    ULONGLONG BytesAllocated;
    ULONG Filler[2];
    ULONGLONG Filler2[7];
} D3DKMTQS_SYSTEM_MEMORY;

typedef struct _D3DKMTQS_PROCESS_INFO
{
    ULONG Filler[2];
    struct {
        ULONGLONG BytesAllocated;

        ULONG Filler[2];
        ULONGLONG Filler2[7];
    } SystemMemory;
    ULONG64 Reserved[8];
} D3DKMTQS_PROCESS_INFO;

typedef struct _D3DKMTQS_PROCESS_SEGMENT_INFO
{
    union {
        struct {
            ULONGLONG BytesCommitted;
        } Win8;
        struct {
            ULONG BytesCommitted;
            ULONG UnknownRandomness;
        } Win7;
    };

    ULONGLONG Filler[2];
    ULONG Filler2;
    struct {
        ULONG Filler;
        D3DKMTQS_COUNTER Filler2[6];
        ULONGLONG Filler3;
    } Filler3;
    struct {
        ULONGLONG Filler;
    } Filler4;
    ULONG64 Reserved[8];
} D3DKMTQS_PROCESS_SEGMENT_INFO;

typedef enum _D3DKMTQS_TYPE
{
    D3DKMTQS_ADAPTER = 0,
    D3DKMTQS_PROCESS = 1,
    D3DKMTQS_SEGMENT = 3,
    D3DKMTQS_PROCESS_SEGMENT = 4,
} D3DKMTQS_TYPE;

typedef union _D3DKMTQS_RESULT
{
    D3DKMTQS_ADAPTER_INFO AdapterInfo;
    D3DKMTQS_SEGMENT_INFO_WIN7 SegmentInfoWin7;
    D3DKMTQS_SEGMENT_INFO_WIN8 SegmentInfoWin8;
    D3DKMTQS_PROCESS_INFO ProcessInfo;
    D3DKMTQS_PROCESS_SEGMENT_INFO ProcessSegmentInfo;
} D3DKMTQS_RESULT;

typedef struct _D3DKMTQS_QUERY_SEGMENT
{
    ULONG SegmentId;
} D3DKMTQS_QUERY_SEGMENT;

typedef struct _D3DKMTQS
{
    D3DKMTQS_TYPE Type;
    LUID AdapterLuid;
    HANDLE hProcess;
    D3DKMTQS_RESULT QueryResult;

    union
    {
        D3DKMTQS_QUERY_SEGMENT QuerySegment;
        D3DKMTQS_QUERY_SEGMENT QueryProcessSegment;
    };
} D3DKMTQS;

extern "C" {
typedef __checkReturn NTSTATUS (APIENTRY *PFND3DKMTQS)(const D3DKMTQS *);
}
