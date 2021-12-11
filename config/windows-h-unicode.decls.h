/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains a series of C-style function prototypes for A/W-suffixed
 * Win32 APIs defined by windows.h.
 *
 * This file is processed by make-windows-h-wrapper.py to generate a wrapper for
 * the header which removes the defines usually implementing these aliases.
 *
 * Wrappers defined in this file will have the 'stdcall' calling convention,
 * will be defined as 'inline', and will only be defined if the corresponding
 * #define directive has not been #undef-ed.
 *
 * NOTE: This is *NOT* a real C header, but rather an input to the avove script.
 * Only basic declarations in the form found here are allowed.
 */

LPTSTR GetCommandLine();

BOOL FreeEnvironmentStrings(LPTCH);

DWORD GetEnvironmentVariable(LPCTSTR, LPTSTR, DWORD);

BOOL SetEnvironmentVariable(LPCTSTR, LPCTSTR);

DWORD ExpandEnvironmentStrings(LPCTSTR, LPTSTR, DWORD);

BOOL SetCurrentDirectory(LPCTSTR);

DWORD GetCurrentDirectory(DWORD, LPTSTR);

DWORD SearchPath(LPCTSTR, LPCTSTR, LPCTSTR, DWORD, LPTSTR, LPTSTR*);

BOOL NeedCurrentDirectoryForExePath(LPCTSTR);

BOOL CreateDirectory(LPCTSTR, LPSECURITY_ATTRIBUTES);

HANDLE CreateFile(LPCTSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                  HANDLE);

BOOL DeleteFile(LPCTSTR);

HANDLE FindFirstChangeNotification(LPCTSTR, BOOL, DWORD);

HANDLE FindFirstFile(LPCTSTR, LPWIN32_FIND_DATA);

HANDLE FindFirstFileEx(LPCTSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS,
                       LPVOID, DWORD);

BOOL FindNextFile(HANDLE, LPWIN32_FIND_DATA);

BOOL GetDiskFreeSpace(LPCTSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD);

BOOL GetDiskFreeSpaceEx(LPCTSTR, PULARGE_INTEGER, PULARGE_INTEGER,
                        PULARGE_INTEGER);

UINT GetDriveType(LPCTSTR);

DWORD GetFileAttributes(LPCTSTR);

BOOL GetFileAttributesEx(LPCTSTR, GET_FILEEX_INFO_LEVELS, LPVOID);

DWORD GetFinalPathNameByHandle(HANDLE, LPTSTR, DWORD, DWORD);

DWORD GetFullPathName(LPCTSTR, DWORD, LPTSTR, LPTSTR*);

DWORD GetLongPathName(LPCTSTR, LPTSTR, DWORD);

BOOL RemoveDirectory(LPCTSTR);

BOOL SetFileAttributes(LPCTSTR, DWORD);

DWORD GetCompressedFileSize(LPCTSTR, LPDWORD);

DWORD GetTempPath(DWORD, LPTSTR);

BOOL GetVolumeInformation(LPCTSTR, LPTSTR, DWORD, LPDWORD, LPDWORD, LPDWORD,
                          LPTSTR, DWORD);

UINT GetTempFileName(LPCTSTR, LPCTSTR, UINT, LPTSTR);

void OutputDebugString(LPCTSTR);

void FatalAppExit(UINT, LPCTSTR);

HANDLE CreateMutex(LPSECURITY_ATTRIBUTES, BOOL, LPCTSTR);

HANDLE CreateEvent(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCTSTR);

HANDLE OpenEvent(DWORD, BOOL, LPCTSTR);

HANDLE CreateMutexEx(LPSECURITY_ATTRIBUTES, LPCTSTR, DWORD, DWORD);

HANDLE CreateEventEx(LPSECURITY_ATTRIBUTES, LPCTSTR, DWORD, DWORD);

BOOL CreateProcess(LPCTSTR, LPTSTR, LPSECURITY_ATTRIBUTES,
                   LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCTSTR,
                   LPSTARTUPINFO, LPPROCESS_INFORMATION);

BOOL CreateProcessAsUser(HANDLE, LPCTSTR, LPTSTR, LPSECURITY_ATTRIBUTES,
                         LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCTSTR,
                         LPSTARTUPINFO, LPPROCESS_INFORMATION);

UINT GetSystemDirectory(LPTSTR, UINT);

UINT GetWindowsDirectory(LPTSTR, UINT);

UINT GetSystemWindowsDirectory(LPTSTR, UINT);

BOOL GetComputerNameEx(COMPUTER_NAME_FORMAT, LPTSTR, LPDWORD);

BOOL GetVersionEx(LPOSVERSIONINFO);

BOOL SetComputerName(LPCTSTR);

BOOL SetComputerNameEx(COMPUTER_NAME_FORMAT, LPCTSTR);

BOOL LoadEnclaveImage(LPVOID, LPCTSTR);

UINT GetSystemWow64Directory(LPTSTR, UINT);

DWORD GetModuleFileName(HMODULE, LPTSTR, DWORD);

HMODULE GetModuleHandle(LPCTSTR);

BOOL GetModuleHandleEx(DWORD, LPCTSTR, HMODULE*);

HMODULE LoadLibraryEx(LPCTSTR, HANDLE, DWORD);

int LoadString(HINSTANCE, UINT, LPTSTR, int);

BOOL EnumResourceLanguagesEx(HMODULE, LPCTSTR, LPCTSTR, ENUMRESLANGPROC,
                             LONG_PTR, DWORD, LANGID);

BOOL EnumResourceNamesEx(HMODULE, LPCTSTR, ENUMRESNAMEPROC, LONG_PTR, DWORD,
                         LANGID);

BOOL EnumResourceTypesEx(HMODULE, ENUMRESTYPEPROC, LONG_PTR, DWORD, LANGID);

HMODULE LoadLibrary(LPCTSTR);

BOOL GetBinaryType(LPCTSTR, LPDWORD);

DWORD GetShortPathName(LPCTSTR, LPTSTR, DWORD);

DWORD GetLongPathNameTransacted(LPCTSTR, LPTSTR, DWORD, HANDLE);

BOOL SetEnvironmentStrings(LPTCH);

BOOL SetFileShortName(HANDLE, LPCTSTR);

DWORD FormatMessage(DWORD, LPCVOID, DWORD, DWORD, LPTSTR, DWORD, va_list*);

HANDLE CreateMailslot(LPCTSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES);

BOOL EncryptFile(LPCTSTR);

BOOL DecryptFile(LPCTSTR, DWORD);

BOOL FileEncryptionStatus(LPCTSTR, LPDWORD);

DWORD OpenEncryptedFileRaw(LPCTSTR, ULONG, PVOID*);

HANDLE OpenMutex(DWORD, BOOL, LPCTSTR);

HANDLE CreateSemaphore(LPSECURITY_ATTRIBUTES, LONG, LONG, LPCTSTR);

HANDLE OpenSemaphore(DWORD, BOOL, LPCTSTR);

HANDLE CreateWaitableTimer(LPSECURITY_ATTRIBUTES, BOOL, LPCTSTR);

HANDLE OpenWaitableTimer(DWORD, BOOL, LPCTSTR);

HANDLE CreateSemaphoreEx(LPSECURITY_ATTRIBUTES, LONG, LONG, LPCTSTR, DWORD,
                         DWORD);

HANDLE CreateWaitableTimerEx(LPSECURITY_ATTRIBUTES, LPCTSTR, DWORD, DWORD);

HANDLE CreateFileMapping(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD,
                         LPCTSTR);

HANDLE CreateFileMappingNuma(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD,
                             LPCTSTR, DWORD);

HANDLE OpenFileMapping(DWORD, BOOL, LPCTSTR);

DWORD GetLogicalDriveStrings(DWORD, LPTSTR);

void GetStartupInfo(LPSTARTUPINFO);

DWORD GetFirmwareEnvironmentVariable(LPCTSTR, LPCTSTR, PVOID, DWORD);

BOOL SetFirmwareEnvironmentVariable(LPCTSTR, LPCTSTR, PVOID, DWORD);

HRSRC FindResource(HMODULE, LPCTSTR, LPCTSTR);

HRSRC FindResourceEx(HMODULE, LPCTSTR, LPCTSTR, WORD);

BOOL EnumResourceTypes(HMODULE, ENUMRESTYPEPROC, LONG_PTR);

BOOL EnumResourceNames(HMODULE, LPCTSTR, ENUMRESNAMEPROC, LONG_PTR);

BOOL EnumResourceLanguages(HMODULE, LPCTSTR, LPCTSTR, ENUMRESLANGPROC,
                           LONG_PTR);

HANDLE BeginUpdateResource(LPCTSTR, BOOL);

BOOL UpdateResource(HANDLE, LPCTSTR, LPCTSTR, WORD, LPVOID, DWORD);

BOOL EndUpdateResource(HANDLE, BOOL);

ATOM GlobalAddAtom(LPCTSTR);

ATOM GlobalAddAtomEx(LPCTSTR, DWORD);

ATOM GlobalFindAtom(LPCTSTR);

UINT GlobalGetAtomName(ATOM, LPTSTR, int);

ATOM AddAtom(LPCTSTR);

ATOM FindAtom(LPCTSTR);

UINT GetAtomName(ATOM, LPTSTR, int);

UINT GetProfileInt(LPCTSTR, LPCTSTR, INT);

DWORD GetProfileString(LPCTSTR, LPCTSTR, LPCTSTR, LPTSTR, DWORD);

BOOL WriteProfileString(LPCTSTR, LPCTSTR, LPCTSTR);

DWORD GetProfileSection(LPCTSTR, LPTSTR, DWORD);

BOOL WriteProfileSection(LPCTSTR, LPCTSTR);

UINT GetPrivateProfileInt(LPCTSTR, LPCTSTR, INT, LPCTSTR);

DWORD GetPrivateProfileString(LPCTSTR, LPCTSTR, LPCTSTR, LPTSTR, DWORD,
                              LPCTSTR);

BOOL WritePrivateProfileString(LPCTSTR, LPCTSTR, LPCTSTR, LPCTSTR);

DWORD GetPrivateProfileSection(LPCTSTR, LPTSTR, DWORD, LPCTSTR);

BOOL WritePrivateProfileSection(LPCTSTR, LPCTSTR, LPCTSTR);

DWORD GetPrivateProfileSectionNames(LPTSTR, DWORD, LPCTSTR);

BOOL GetPrivateProfileStruct(LPCTSTR, LPCTSTR, LPVOID, UINT, LPCTSTR);

BOOL WritePrivateProfileStruct(LPCTSTR, LPCTSTR, LPVOID, UINT, LPCTSTR);

BOOL SetDllDirectory(LPCTSTR);

DWORD GetDllDirectory(DWORD, LPTSTR);

BOOL CreateDirectoryEx(LPCTSTR, LPCTSTR, LPSECURITY_ATTRIBUTES);

BOOL CreateDirectoryTransacted(LPCTSTR, LPCTSTR, LPSECURITY_ATTRIBUTES, HANDLE);

BOOL RemoveDirectoryTransacted(LPCTSTR, HANDLE);

DWORD GetFullPathNameTransacted(LPCTSTR, DWORD, LPTSTR, LPTSTR*, HANDLE);

BOOL DefineDosDevice(DWORD, LPCTSTR, LPCTSTR);

DWORD QueryDosDevice(LPCTSTR, LPTSTR, DWORD);

HANDLE CreateFileTransacted(LPCTSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD,
                            DWORD, HANDLE, HANDLE, PUSHORT, PVOID);

BOOL SetFileAttributesTransacted(LPCTSTR, DWORD, HANDLE);

BOOL GetFileAttributesTransacted(LPCTSTR, GET_FILEEX_INFO_LEVELS, LPVOID,
                                 HANDLE);

DWORD GetCompressedFileSizeTransacted(LPCTSTR, LPDWORD, HANDLE);

BOOL DeleteFileTransacted(LPCTSTR, HANDLE);

BOOL CheckNameLegalDOS8Dot3(LPCTSTR, LPSTR, DWORD, PBOOL, PBOOL);

HANDLE FindFirstFileTransacted(LPCTSTR, FINDEX_INFO_LEVELS, LPVOID,
                               FINDEX_SEARCH_OPS, LPVOID, DWORD, HANDLE);

BOOL CopyFile(LPCTSTR, LPCTSTR, BOOL);

BOOL CopyFileEx(LPCTSTR, LPCTSTR, LPPROGRESS_ROUTINE, LPVOID, LPBOOL, DWORD);

BOOL CopyFileTransacted(LPCTSTR, LPCTSTR, LPPROGRESS_ROUTINE, LPVOID, LPBOOL,
                        DWORD, HANDLE);

BOOL MoveFile(LPCTSTR, LPCTSTR);

BOOL MoveFileEx(LPCTSTR, LPCTSTR, DWORD);

BOOL MoveFileWithProgress(LPCTSTR, LPCTSTR, LPPROGRESS_ROUTINE, LPVOID, DWORD);

BOOL MoveFileTransacted(LPCTSTR, LPCTSTR, LPPROGRESS_ROUTINE, LPVOID, DWORD,
                        HANDLE);

BOOL ReplaceFile(LPCTSTR, LPCTSTR, LPCTSTR, DWORD, LPVOID, LPVOID);

BOOL CreateHardLink(LPCTSTR, LPCTSTR, LPSECURITY_ATTRIBUTES);

BOOL CreateHardLinkTransacted(LPCTSTR, LPCTSTR, LPSECURITY_ATTRIBUTES, HANDLE);

HANDLE CreateNamedPipe(LPCTSTR, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                       LPSECURITY_ATTRIBUTES);

BOOL GetNamedPipeHandleState(HANDLE, LPDWORD, LPDWORD, LPDWORD, LPDWORD, LPTSTR,
                             DWORD);

BOOL CallNamedPipe(LPCTSTR, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, DWORD);

BOOL WaitNamedPipe(LPCTSTR, DWORD);

BOOL GetNamedPipeClientComputerName(HANDLE, LPTSTR, ULONG);

BOOL SetVolumeLabel(LPCTSTR, LPCTSTR);

BOOL ClearEventLog(HANDLE, LPCTSTR);

BOOL BackupEventLog(HANDLE, LPCTSTR);

HANDLE OpenEventLog(LPCTSTR, LPCTSTR);

HANDLE RegisterEventSource(LPCTSTR, LPCTSTR);

HANDLE OpenBackupEventLog(LPCTSTR, LPCTSTR);

BOOL ReadEventLog(HANDLE, DWORD, DWORD, LPVOID, DWORD, DWORD*, DWORD*);

BOOL ReportEvent(HANDLE, WORD, WORD, DWORD, PSID, WORD, DWORD, LPCTSTR*,
                 LPVOID);

BOOL AccessCheckAndAuditAlarm(LPCTSTR, LPVOID, LPTSTR, LPTSTR,
                              PSECURITY_DESCRIPTOR, DWORD, PGENERIC_MAPPING,
                              BOOL, LPDWORD, LPBOOL, LPBOOL);

BOOL AccessCheckByTypeAndAuditAlarm(LPCTSTR, LPVOID, LPCTSTR, LPCTSTR,
                                    PSECURITY_DESCRIPTOR, PSID, DWORD,
                                    AUDIT_EVENT_TYPE, DWORD, POBJECT_TYPE_LIST,
                                    DWORD, PGENERIC_MAPPING, BOOL, LPDWORD,
                                    LPBOOL, LPBOOL);

BOOL AccessCheckByTypeResultListAndAuditAlarm(LPCTSTR, LPVOID, LPCTSTR, LPCTSTR,
                                              PSECURITY_DESCRIPTOR, PSID, DWORD,
                                              AUDIT_EVENT_TYPE, DWORD,
                                              POBJECT_TYPE_LIST, DWORD,
                                              PGENERIC_MAPPING, BOOL, LPDWORD,
                                              LPDWORD, LPBOOL);

BOOL AccessCheckByTypeResultListAndAuditAlarmByHandle(
    LPCTSTR, LPVOID, HANDLE, LPCTSTR, LPCTSTR, PSECURITY_DESCRIPTOR, PSID,
    DWORD, AUDIT_EVENT_TYPE, DWORD, POBJECT_TYPE_LIST, DWORD, PGENERIC_MAPPING,
    BOOL, LPDWORD, LPDWORD, LPBOOL);

BOOL ObjectOpenAuditAlarm(LPCTSTR, LPVOID, LPTSTR, LPTSTR, PSECURITY_DESCRIPTOR,
                          HANDLE, DWORD, DWORD, PPRIVILEGE_SET, BOOL, BOOL,
                          LPBOOL);

BOOL ObjectPrivilegeAuditAlarm(LPCTSTR, LPVOID, HANDLE, DWORD, PPRIVILEGE_SET,
                               BOOL);

BOOL ObjectCloseAuditAlarm(LPCTSTR, LPVOID, BOOL);

BOOL ObjectDeleteAuditAlarm(LPCTSTR, LPVOID, BOOL);

BOOL PrivilegedServiceAuditAlarm(LPCTSTR, LPCTSTR, HANDLE, PPRIVILEGE_SET,
                                 BOOL);

BOOL SetFileSecurity(LPCTSTR, SECURITY_INFORMATION, PSECURITY_DESCRIPTOR);

BOOL GetFileSecurity(LPCTSTR, SECURITY_INFORMATION, PSECURITY_DESCRIPTOR, DWORD,
                     LPDWORD);

BOOL IsBadStringPtr(LPCTSTR, UINT_PTR);

BOOL LookupAccountSid(LPCTSTR, PSID, LPTSTR, LPDWORD, LPTSTR, LPDWORD,
                      PSID_NAME_USE);

BOOL LookupAccountName(LPCTSTR, LPCTSTR, PSID, LPDWORD, LPTSTR, LPDWORD,
                       PSID_NAME_USE);

BOOL LookupAccountNameLocal(LPCTSTR, PSID, LPDWORD, LPTSTR, LPDWORD,
                            PSID_NAME_USE);

BOOL LookupAccountSidLocal(PSID, LPTSTR, LPDWORD, LPTSTR, LPDWORD,
                           PSID_NAME_USE);

BOOL LookupPrivilegeValue(LPCTSTR, LPCTSTR, PLUID);

BOOL LookupPrivilegeName(LPCTSTR, PLUID, LPTSTR, LPDWORD);

BOOL LookupPrivilegeDisplayName(LPCTSTR, LPCTSTR, LPTSTR, LPDWORD, LPDWORD);

BOOL BuildCommDCB(LPCTSTR, LPDCB);

BOOL BuildCommDCBAndTimeouts(LPCTSTR, LPDCB, LPCOMMTIMEOUTS);

BOOL CommConfigDialog(LPCTSTR, HWND, LPCOMMCONFIG);

BOOL GetDefaultCommConfig(LPCTSTR, LPCOMMCONFIG, LPDWORD);

BOOL SetDefaultCommConfig(LPCTSTR, LPCOMMCONFIG, DWORD);

BOOL GetComputerName(LPTSTR, LPDWORD);

BOOL DnsHostnameToComputerName(LPCTSTR, LPTSTR, LPDWORD);

BOOL GetUserName(LPTSTR, LPDWORD);

BOOL LogonUser(LPCTSTR, LPCTSTR, LPCTSTR, DWORD, DWORD, PHANDLE);

BOOL LogonUserEx(LPCTSTR, LPCTSTR, LPCTSTR, DWORD, DWORD, PHANDLE, PSID*,
                 PVOID*, LPDWORD, PQUOTA_LIMITS);

HANDLE CreatePrivateNamespace(LPSECURITY_ATTRIBUTES, LPVOID, LPCTSTR);

HANDLE OpenPrivateNamespace(LPVOID, LPCTSTR);

HANDLE CreateBoundaryDescriptor(LPCTSTR, ULONG);

BOOL GetCurrentHwProfile(LPHW_PROFILE_INFO);

BOOL VerifyVersionInfo(LPOSVERSIONINFOEX, DWORD, DWORDLONG);

HANDLE CreateJobObject(LPSECURITY_ATTRIBUTES, LPCTSTR);

HANDLE OpenJobObject(DWORD, BOOL, LPCTSTR);

HANDLE FindFirstVolume(LPTSTR, DWORD);

BOOL FindNextVolume(HANDLE, LPTSTR, DWORD);

HANDLE FindFirstVolumeMountPoint(LPCTSTR, LPTSTR, DWORD);

BOOL FindNextVolumeMountPoint(HANDLE, LPTSTR, DWORD);

BOOL SetVolumeMountPoint(LPCTSTR, LPCTSTR);

BOOL DeleteVolumeMountPoint(LPCTSTR);

BOOL GetVolumeNameForVolumeMountPoint(LPCTSTR, LPTSTR, DWORD);

BOOL GetVolumePathName(LPCTSTR, LPTSTR, DWORD);

BOOL GetVolumePathNamesForVolumeName(LPCTSTR, LPTCH, DWORD, PDWORD);

HANDLE CreateActCtx(PCACTCTX);

BOOL FindActCtxSectionString(DWORD, const GUID*, ULONG, LPCTSTR,
                             PACTCTX_SECTION_KEYED_DATA);

BOOLEAN CreateSymbolicLink(LPCTSTR, LPCTSTR, DWORD);

BOOLEAN CreateSymbolicLinkTransacted(LPCTSTR, LPCTSTR, DWORD, HANDLE);

int AddFontResource(LPCTSTR);

HMETAFILE CopyMetaFile(HMETAFILE, LPCTSTR);

HDC CreateDC(LPCTSTR, LPCTSTR, LPCTSTR, const DEVMODE*);

HFONT CreateFontIndirect(const LOGFONT*);

HFONT CreateFont(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD,
                 DWORD, DWORD, DWORD, LPCTSTR);

HDC CreateIC(LPCTSTR, LPCTSTR, LPCTSTR, const DEVMODE*);

HDC CreateMetaFile(LPCTSTR);

BOOL CreateScalableFontResource(DWORD, LPCTSTR, LPCTSTR, LPCTSTR);

int DeviceCapabilities(LPCTSTR, LPCTSTR, WORD, LPTSTR, const DEVMODE*);

int EnumFontFamiliesEx(HDC, LPLOGFONT, FONTENUMPROC, LPARAM, DWORD);

int EnumFontFamilies(HDC, LPCTSTR, FONTENUMPROC, LPARAM);

int EnumFonts(HDC, LPCTSTR, FONTENUMPROC, LPARAM);

BOOL GetCharWidth(HDC, UINT, UINT, LPINT);

BOOL GetCharWidth32(HDC, UINT, UINT, LPINT);

BOOL GetCharWidthFloat(HDC, UINT, UINT, PFLOAT);

BOOL GetCharABCWidths(HDC, UINT, UINT, LPABC);

BOOL GetCharABCWidthsFloat(HDC, UINT, UINT, LPABCFLOAT);

DWORD GetGlyphOutline(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID,
                      const MAT2*);

HMETAFILE GetMetaFile(LPCTSTR);

UINT GetOutlineTextMetrics(HDC, UINT, LPOUTLINETEXTMETRIC);

BOOL GetTextExtentPoint(HDC, LPCTSTR, int, LPSIZE);

BOOL GetTextExtentPoint32(HDC, LPCTSTR, int, LPSIZE);

BOOL GetTextExtentExPoint(HDC, LPCTSTR, int, int, LPINT, LPINT, LPSIZE);

DWORD GetCharacterPlacement(HDC, LPCTSTR, int, int, LPGCP_RESULTS, DWORD);

DWORD GetGlyphIndices(HDC, LPCTSTR, int, LPWORD, DWORD);

int AddFontResourceEx(LPCTSTR, DWORD, PVOID);

BOOL RemoveFontResourceEx(LPCTSTR, DWORD, PVOID);

HFONT CreateFontIndirectEx(const ENUMLOGFONTEXDV*);

HDC ResetDC(HDC, const DEVMODE*);

BOOL RemoveFontResource(LPCTSTR);

HENHMETAFILE CopyEnhMetaFile(HENHMETAFILE, LPCTSTR);

HDC CreateEnhMetaFile(HDC, LPCTSTR, const RECT*, LPCTSTR);

HENHMETAFILE GetEnhMetaFile(LPCTSTR);

UINT GetEnhMetaFileDescription(HENHMETAFILE, UINT, LPTSTR);

BOOL GetTextMetrics(HDC, LPTEXTMETRIC);

int StartDoc(HDC, const DOCINFO*);

int GetObject(HANDLE, int, LPVOID);

BOOL TextOut(HDC, int, int, LPCTSTR, int);

BOOL ExtTextOut(HDC, int, int, UINT, const RECT*, LPCTSTR, UINT, const INT*);

BOOL PolyTextOut(HDC, const POLYTEXT*, int);

int GetTextFace(HDC, int, LPTSTR);

DWORD GetKerningPairs(HDC, DWORD, LPKERNINGPAIR);

BOOL GetLogColorSpace(HCOLORSPACE, LPLOGCOLORSPACE, DWORD);

HCOLORSPACE CreateColorSpace(LPLOGCOLORSPACE);

BOOL GetICMProfile(HDC, LPDWORD, LPTSTR);

BOOL SetICMProfile(HDC, LPTSTR);

int EnumICMProfiles(HDC, ICMENUMPROC, LPARAM);

BOOL UpdateICMRegKey(DWORD, LPTSTR, LPTSTR, UINT);

HKL LoadKeyboardLayout(LPCTSTR, UINT);

BOOL GetKeyboardLayoutName(LPTSTR);

HDESK CreateDesktop(LPCTSTR, LPCTSTR, DEVMODE*, DWORD, ACCESS_MASK,
                    LPSECURITY_ATTRIBUTES);

HDESK CreateDesktopEx(LPCTSTR, LPCTSTR, DEVMODE*, DWORD, ACCESS_MASK,
                      LPSECURITY_ATTRIBUTES, ULONG, PVOID);

HDESK OpenDesktop(LPCTSTR, DWORD, BOOL, ACCESS_MASK);

BOOL EnumDesktops(HWINSTA, DESKTOPENUMPROC, LPARAM);

HWINSTA CreateWindowStation(LPCTSTR, DWORD, ACCESS_MASK, LPSECURITY_ATTRIBUTES);

HWINSTA OpenWindowStation(LPCTSTR, BOOL, ACCESS_MASK);

BOOL EnumWindowStations(WINSTAENUMPROC, LPARAM);

BOOL GetUserObjectInformation(HANDLE, int, PVOID, DWORD, LPDWORD);

BOOL SetUserObjectInformation(HANDLE, int, PVOID, DWORD);

UINT RegisterWindowMessage(LPCTSTR);

BOOL GetMessage(LPMSG, HWND, UINT, UINT);

LRESULT DispatchMessage(const MSG*);

BOOL PeekMessage(LPMSG, HWND, UINT, UINT, UINT);

LRESULT SendMessage(HWND, UINT, WPARAM, LPARAM);

LRESULT SendMessageTimeout(HWND, UINT, WPARAM, LPARAM, UINT, UINT, PDWORD_PTR);

BOOL SendNotifyMessage(HWND, UINT, WPARAM, LPARAM);

BOOL SendMessageCallback(HWND, UINT, WPARAM, LPARAM, SENDASYNCPROC, ULONG_PTR);

long BroadcastSystemMessageEx(DWORD, LPDWORD, UINT, WPARAM, LPARAM, PBSMINFO);

long BroadcastSystemMessage(DWORD, LPDWORD, UINT, WPARAM, LPARAM);

HDEVNOTIFY RegisterDeviceNotification(HANDLE, LPVOID, DWORD);

BOOL PostMessage(HWND, UINT, WPARAM, LPARAM);

BOOL PostThreadMessage(DWORD, UINT, WPARAM, LPARAM);

BOOL PostAppMessage(DWORD, UINT, WPARAM, LPARAM);

LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM);

LRESULT CallWindowProc(WNDPROC, HWND, UINT, WPARAM, LPARAM);

ATOM RegisterClass(const WNDCLASS*);

BOOL UnregisterClass(LPCTSTR, HINSTANCE);

BOOL GetClassInfo(HINSTANCE, LPCTSTR, LPWNDCLASS);

ATOM RegisterClassEx(const WNDCLASSEX*);

BOOL GetClassInfoEx(HINSTANCE, LPCTSTR, LPWNDCLASSEX);

HWND CreateWindowEx(DWORD, LPCTSTR, LPCTSTR, DWORD, int, int, int, int, HWND,
                    HMENU, HINSTANCE, LPVOID);

HWND CreateWindow(LPCTSTR, LPCTSTR, DWORD, int, int, int, int, HWND, HMENU,
                  HINSTANCE, LPVOID);

HWND CreateDialogParam(HINSTANCE, LPCTSTR, HWND, DLGPROC, LPARAM);

HWND CreateDialogIndirectParam(HINSTANCE, LPCDLGTEMPLATE, HWND, DLGPROC,
                               LPARAM);

HWND CreateDialog(HINSTANCE, LPCTSTR, HWND, DLGPROC);

HWND CreateDialogIndirect(HINSTANCE, LPCDLGTEMPLATE, HWND, DLGPROC);

INT_PTR DialogBoxParam(HINSTANCE, LPCTSTR, HWND, DLGPROC, LPARAM);

INT_PTR DialogBoxIndirectParam(HINSTANCE, LPCDLGTEMPLATE, HWND, DLGPROC,
                               LPARAM);

INT_PTR DialogBox(HINSTANCE, LPCTSTR, HWND, DLGPROC);

INT_PTR DialogBoxIndirect(HINSTANCE, LPCDLGTEMPLATE, HWND, DLGPROC);

BOOL SetDlgItemText(HWND, int, LPCTSTR);

UINT GetDlgItemText(HWND, int, LPTSTR, int);

LRESULT SendDlgItemMessage(HWND, int, UINT, WPARAM, LPARAM);

LRESULT DefDlgProc(HWND, UINT, WPARAM, LPARAM);

BOOL CallMsgFilter(LPMSG, int);

UINT RegisterClipboardFormat(LPCTSTR);

int GetClipboardFormatName(UINT, LPTSTR, int);

BOOL CharToOem(LPCTSTR, LPSTR);

BOOL OemToChar(LPCSTR, LPTSTR);

BOOL CharToOemBuff(LPCTSTR, LPSTR, DWORD);

BOOL OemToCharBuff(LPCSTR, LPTSTR, DWORD);

LPTSTR CharUpper(LPTSTR);

DWORD CharUpperBuff(LPTSTR, DWORD);

LPTSTR CharLower(LPTSTR);

DWORD CharLowerBuff(LPTSTR, DWORD);

LPTSTR CharNext(LPCTSTR);

LPTSTR CharPrev(LPCTSTR, LPCTSTR);

BOOL IsCharAlpha(CHAR);

BOOL IsCharAlphaNumeric(CHAR);

BOOL IsCharUpper(CHAR);

BOOL IsCharLower(CHAR);

int GetKeyNameText(LONG, LPTSTR, int);

SHORT VkKeyScan(CHAR);

SHORT VkKeyScanEx(CHAR, HKL);

UINT MapVirtualKey(UINT, UINT);

UINT MapVirtualKeyEx(UINT, UINT, HKL);

HACCEL LoadAccelerators(HINSTANCE, LPCTSTR);

HACCEL CreateAcceleratorTable(LPACCEL, int);

int CopyAcceleratorTable(HACCEL, LPACCEL, int);

int TranslateAccelerator(HWND, HACCEL, LPMSG);

HMENU LoadMenu(HINSTANCE, LPCTSTR);

HMENU LoadMenuIndirect(const MENUTEMPLATE*);

BOOL ChangeMenu(HMENU, UINT, LPCTSTR, UINT, UINT);

int GetMenuString(HMENU, UINT, LPTSTR, int, UINT);

BOOL InsertMenu(HMENU, UINT, UINT, UINT_PTR, LPCTSTR);

BOOL AppendMenu(HMENU, UINT, UINT_PTR, LPCTSTR);

BOOL ModifyMenu(HMENU, UINT, UINT, UINT_PTR, LPCTSTR);

BOOL InsertMenuItem(HMENU, UINT, BOOL, LPCMENUITEMINFO);

BOOL GetMenuItemInfo(HMENU, UINT, BOOL, LPMENUITEMINFO);

BOOL SetMenuItemInfo(HMENU, UINT, BOOL, LPCMENUITEMINFO);

int DrawText(HDC, LPCTSTR, int, LPRECT, UINT);

int DrawTextEx(HDC, LPTSTR, int, LPRECT, UINT, LPDRAWTEXTPARAMS);

BOOL GrayString(HDC, HBRUSH, GRAYSTRINGPROC, LPARAM, int, int, int, int, int);

BOOL DrawState(HDC, HBRUSH, DRAWSTATEPROC, LPARAM, WPARAM, int, int, int, int,
               UINT);

LONG TabbedTextOut(HDC, int, int, LPCTSTR, int, int, const INT*, int);

DWORD GetTabbedTextExtent(HDC, LPCTSTR, int, int, const INT*);

BOOL SetProp(HWND, LPCTSTR, HANDLE);

HANDLE GetProp(HWND, LPCTSTR);

HANDLE RemoveProp(HWND, LPCTSTR);

int EnumPropsEx(HWND, PROPENUMPROCEX, LPARAM);

int EnumProps(HWND, PROPENUMPROC);

BOOL SetWindowText(HWND, LPCTSTR);

int GetWindowText(HWND, LPTSTR, int);

int GetWindowTextLength(HWND);

int MessageBox(HWND, LPCTSTR, LPCTSTR, UINT);

int MessageBoxEx(HWND, LPCTSTR, LPCTSTR, UINT, WORD);

int MessageBoxIndirect(const MSGBOXPARAMS*);

LONG GetWindowLong(HWND, int);

LONG SetWindowLong(HWND, int, LONG);

LONG_PTR GetWindowLongPtr(HWND, int);

LONG_PTR SetWindowLongPtr(HWND, int, LONG_PTR);

DWORD GetClassLong(HWND, int);

DWORD SetClassLong(HWND, int, LONG);

ULONG_PTR GetClassLongPtr(HWND, int);

ULONG_PTR SetClassLongPtr(HWND, int, LONG_PTR);

HWND FindWindow(LPCTSTR, LPCTSTR);

HWND FindWindowEx(HWND, HWND, LPCTSTR, LPCTSTR);

int GetClassName(HWND, LPTSTR, int);

HHOOK SetWindowsHook(int, HOOKPROC);

HHOOK SetWindowsHookEx(int, HOOKPROC, HINSTANCE, DWORD);

HBITMAP LoadBitmap(HINSTANCE, LPCTSTR);

HCURSOR LoadCursor(HINSTANCE, LPCTSTR);

HCURSOR LoadCursorFromFile(LPCTSTR);

HICON LoadIcon(HINSTANCE, LPCTSTR);

UINT PrivateExtractIcons(LPCTSTR, int, int, int, HICON*, UINT*, UINT, UINT);

HANDLE LoadImage(HINSTANCE, LPCTSTR, UINT, int, int, UINT);

BOOL GetIconInfoEx(HICON, PICONINFOEX);

BOOL IsDialogMessage(HWND, LPMSG);

int DlgDirList(HWND, LPTSTR, int, int, UINT);

BOOL DlgDirSelectEx(HWND, LPTSTR, int, int);

int DlgDirListComboBox(HWND, LPTSTR, int, int, UINT);

BOOL DlgDirSelectComboBoxEx(HWND, LPTSTR, int, int);

LRESULT DefFrameProc(HWND, HWND, UINT, WPARAM, LPARAM);

LRESULT DefMDIChildProc(HWND, UINT, WPARAM, LPARAM);

HWND CreateMDIWindow(LPCTSTR, LPCTSTR, DWORD, int, int, int, int, HWND,
                     HINSTANCE, LPARAM);

BOOL WinHelp(HWND, LPCTSTR, UINT, ULONG_PTR);

LONG ChangeDisplaySettings(DEVMODE*, DWORD);

LONG ChangeDisplaySettingsEx(LPCTSTR, DEVMODE*, HWND, DWORD, LPVOID);

BOOL EnumDisplaySettings(LPCTSTR, DWORD, DEVMODE*);

BOOL EnumDisplaySettingsEx(LPCTSTR, DWORD, DEVMODE*, DWORD);

BOOL EnumDisplayDevices(LPCTSTR, DWORD, PDISPLAY_DEVICE, DWORD);

BOOL SystemParametersInfo(UINT, UINT, PVOID, UINT);

BOOL GetMonitorInfo(HMONITOR, LPMONITORINFO);

UINT GetWindowModuleFileName(HWND, LPTSTR, UINT);

UINT RealGetWindowClass(HWND, LPTSTR, UINT);

BOOL GetAltTabInfo(HWND, int, PALTTABINFO, LPTSTR, UINT);

UINT GetRawInputDeviceInfo(HANDLE, UINT, LPVOID, PUINT);

int GetDateFormat(LCID, DWORD, const SYSTEMTIME*, LPCTSTR, LPTSTR, int);

int GetTimeFormat(LCID, DWORD, const SYSTEMTIME*, LPCTSTR, LPTSTR, int);

BOOL GetCPInfoEx(UINT, DWORD, LPCPINFOEX);

int CompareString(LCID, DWORD, PCNZTCH, int, PCNZTCH, int);

int GetLocaleInfo(LCID, LCTYPE, LPTSTR, int);

BOOL SetLocaleInfo(LCID, LCTYPE, LPCTSTR);

int GetCalendarInfo(LCID, CALID, CALTYPE, LPTSTR, int, LPDWORD);

BOOL SetCalendarInfo(LCID, CALID, CALTYPE, LPCTSTR);

int GetNumberFormat(LCID, DWORD, LPCTSTR, const NUMBERFMT*, LPTSTR, int);

int GetCurrencyFormat(LCID, DWORD, LPCTSTR, const CURRENCYFMT*, LPTSTR, int);

BOOL EnumCalendarInfo(CALINFO_ENUMPROC, LCID, CALID, CALTYPE);

BOOL EnumCalendarInfoEx(CALINFO_ENUMPROCEX, LCID, CALID, CALTYPE);

BOOL EnumTimeFormats(TIMEFMT_ENUMPROC, LCID, DWORD);

BOOL EnumDateFormats(DATEFMT_ENUMPROC, LCID, DWORD);

BOOL EnumDateFormatsEx(DATEFMT_ENUMPROCEX, LCID, DWORD);

int GetGeoInfo(GEOID, GEOTYPE, LPTSTR, int, LANGID);

BOOL GetStringTypeEx(LCID, DWORD, LPCTSTR, int, LPWORD);

int FoldString(DWORD, LPCTSTR, int, LPTSTR, int);

BOOL EnumSystemLocales(LOCALE_ENUMPROC, DWORD);

BOOL EnumSystemLanguageGroups(LANGUAGEGROUP_ENUMPROC, DWORD, LONG_PTR);

BOOL EnumLanguageGroupLocales(LANGGROUPLOCALE_ENUMPROC, LGRPID, DWORD,
                              LONG_PTR);

BOOL EnumUILanguages(UILANGUAGE_ENUMPROC, DWORD, LONG_PTR);

BOOL EnumSystemCodePages(CODEPAGE_ENUMPROC, DWORD);

BOOL ReadConsoleInput(HANDLE, PINPUT_RECORD, DWORD, LPDWORD);

BOOL PeekConsoleInput(HANDLE, PINPUT_RECORD, DWORD, LPDWORD);

BOOL ReadConsole(HANDLE, LPVOID, DWORD, LPDWORD, PCONSOLE_READCONSOLE_CONTROL);

BOOL WriteConsole(HANDLE, const void*, DWORD, LPDWORD, LPVOID);

BOOL FillConsoleOutputCharacter(HANDLE, CHAR, DWORD, COORD, LPDWORD);

BOOL WriteConsoleOutputCharacter(HANDLE, LPCTSTR, DWORD, COORD, LPDWORD);

BOOL ReadConsoleOutputCharacter(HANDLE, LPTSTR, DWORD, COORD, LPDWORD);

BOOL WriteConsoleInput(HANDLE, const INPUT_RECORD*, DWORD, LPDWORD);

BOOL ScrollConsoleScreenBuffer(HANDLE, const SMALL_RECT*, const SMALL_RECT*,
                               COORD, const CHAR_INFO*);

BOOL WriteConsoleOutput(HANDLE, const CHAR_INFO*, COORD, COORD, PSMALL_RECT);

BOOL ReadConsoleOutput(HANDLE, PCHAR_INFO, COORD, COORD, PSMALL_RECT);

DWORD GetConsoleTitle(LPTSTR, DWORD);

DWORD GetConsoleOriginalTitle(LPTSTR, DWORD);

BOOL SetConsoleTitle(LPCTSTR);

BOOL AddConsoleAlias(LPTSTR, LPTSTR, LPTSTR);

DWORD GetConsoleAlias(LPTSTR, LPTSTR, DWORD, LPTSTR);

DWORD GetConsoleAliasesLength(LPTSTR);

DWORD GetConsoleAliasExesLength();

DWORD GetConsoleAliases(LPTSTR, DWORD, LPTSTR);

DWORD GetConsoleAliasExes(LPTSTR, DWORD);

void ExpungeConsoleCommandHistory(LPTSTR);

BOOL SetConsoleNumberOfCommands(DWORD, LPTSTR);

DWORD GetConsoleCommandHistoryLength(LPTSTR);

DWORD GetConsoleCommandHistory(LPTSTR, DWORD, LPTSTR);

DWORD VerFindFile(DWORD, LPTSTR, LPTSTR, LPTSTR, LPTSTR, PUINT, LPTSTR, PUINT);

DWORD VerInstallFile(DWORD, LPTSTR, LPTSTR, LPTSTR, LPTSTR, LPTSTR, LPTSTR,
                     PUINT);

DWORD GetFileVersionInfoSize(LPCTSTR, LPDWORD);

BOOL GetFileVersionInfo(LPCTSTR, DWORD, DWORD, LPVOID);

DWORD GetFileVersionInfoSizeEx(DWORD, LPCTSTR, LPDWORD);

BOOL GetFileVersionInfoEx(DWORD, LPCTSTR, DWORD, DWORD, LPVOID);

DWORD VerLanguageName(DWORD, LPTSTR, DWORD);

BOOL VerQueryValue(LPCVOID, LPCTSTR, LPVOID*, PUINT);

LSTATUS RegConnectRegistry(LPCTSTR, HKEY, PHKEY);

LSTATUS RegConnectRegistryEx(LPCTSTR, HKEY, ULONG, PHKEY);

LSTATUS RegCreateKey(HKEY, LPCTSTR, PHKEY);

LSTATUS RegCreateKeyEx(HKEY, LPCTSTR, DWORD, LPTSTR, DWORD, REGSAM,
                       const LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);

LSTATUS RegCreateKeyTransacted(HKEY, LPCTSTR, DWORD, LPTSTR, DWORD, REGSAM,
                               const LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD,
                               HANDLE, PVOID);

LSTATUS RegDeleteKey(HKEY, LPCTSTR);

LSTATUS RegDeleteKeyEx(HKEY, LPCTSTR, REGSAM, DWORD);

LSTATUS RegDeleteKeyTransacted(HKEY, LPCTSTR, REGSAM, DWORD, HANDLE, PVOID);

LSTATUS RegDeleteValue(HKEY, LPCTSTR);

LSTATUS RegEnumKey(HKEY, DWORD, LPTSTR, DWORD);

LSTATUS RegEnumKeyEx(HKEY, DWORD, LPTSTR, LPDWORD, LPDWORD, LPTSTR, LPDWORD,
                     PFILETIME);

LSTATUS RegEnumValue(HKEY, DWORD, LPTSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE,
                     LPDWORD);

LSTATUS RegLoadKey(HKEY, LPCTSTR, LPCTSTR);

LSTATUS RegOpenKey(HKEY, LPCTSTR, PHKEY);

LSTATUS RegOpenKeyEx(HKEY, LPCTSTR, DWORD, REGSAM, PHKEY);

LSTATUS RegOpenKeyTransacted(HKEY, LPCTSTR, DWORD, REGSAM, PHKEY, HANDLE,
                             PVOID);

LSTATUS RegQueryInfoKey(HKEY, LPTSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD,
                        LPDWORD, LPDWORD, LPDWORD, LPDWORD, LPDWORD, PFILETIME);

LSTATUS RegQueryValue(HKEY, LPCTSTR, LPTSTR, PLONG);

LSTATUS RegQueryMultipleValues(HKEY, PVALENT, DWORD, LPTSTR, LPDWORD);

LSTATUS RegQueryValueEx(HKEY, LPCTSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);

LSTATUS RegReplaceKey(HKEY, LPCTSTR, LPCTSTR, LPCTSTR);

LSTATUS RegRestoreKey(HKEY, LPCTSTR, DWORD);

LSTATUS RegSaveKey(HKEY, LPCTSTR, const LPSECURITY_ATTRIBUTES);

LSTATUS RegSetValue(HKEY, LPCTSTR, DWORD, LPCTSTR, DWORD);

LSTATUS RegSetValueEx(HKEY, LPCTSTR, DWORD, DWORD, const BYTE*, DWORD);

LSTATUS RegUnLoadKey(HKEY, LPCTSTR);

LSTATUS RegDeleteKeyValue(HKEY, LPCTSTR, LPCTSTR);

LSTATUS RegSetKeyValue(HKEY, LPCTSTR, LPCTSTR, DWORD, LPCVOID, DWORD);

LSTATUS RegDeleteTree(HKEY, LPCTSTR);

LSTATUS RegCopyTree(HKEY, LPCTSTR, HKEY);

LSTATUS RegGetValue(HKEY, LPCTSTR, LPCTSTR, DWORD, LPDWORD, PVOID, LPDWORD);

LSTATUS RegLoadMUIString(HKEY, LPCTSTR, LPTSTR, DWORD, LPDWORD, DWORD, LPCTSTR);

LSTATUS RegLoadAppKey(LPCTSTR, PHKEY, REGSAM, DWORD, DWORD);

BOOL InitiateSystemShutdown(LPTSTR, LPTSTR, DWORD, BOOL, BOOL);

BOOL AbortSystemShutdown(LPTSTR);

BOOL InitiateSystemShutdownEx(LPTSTR, LPTSTR, DWORD, BOOL, BOOL, DWORD);

DWORD InitiateShutdown(LPTSTR, LPTSTR, DWORD, DWORD, DWORD);

LSTATUS RegSaveKeyEx(HKEY, LPCTSTR, const LPSECURITY_ATTRIBUTES, DWORD);

DWORD MultinetGetConnectionPerformance(LPNETRESOURCE, LPNETCONNECTINFOSTRUCT);

BOOL ChangeServiceConfig(SC_HANDLE, DWORD, DWORD, DWORD, LPCTSTR, LPCTSTR,
                         LPDWORD, LPCTSTR, LPCTSTR, LPCTSTR, LPCTSTR);

BOOL ChangeServiceConfig2(SC_HANDLE, DWORD, LPVOID);

SC_HANDLE CreateService(SC_HANDLE, LPCTSTR, LPCTSTR, DWORD, DWORD, DWORD, DWORD,
                        LPCTSTR, LPCTSTR, LPDWORD, LPCTSTR, LPCTSTR, LPCTSTR);

BOOL EnumDependentServices(SC_HANDLE, DWORD, LPENUM_SERVICE_STATUS, DWORD,
                           LPDWORD, LPDWORD);

BOOL EnumServicesStatus(SC_HANDLE, DWORD, DWORD, LPENUM_SERVICE_STATUS, DWORD,
                        LPDWORD, LPDWORD, LPDWORD);

BOOL EnumServicesStatusEx(SC_HANDLE, SC_ENUM_TYPE, DWORD, DWORD, LPBYTE, DWORD,
                          LPDWORD, LPDWORD, LPDWORD, LPCTSTR);

BOOL GetServiceKeyName(SC_HANDLE, LPCTSTR, LPTSTR, LPDWORD);

BOOL GetServiceDisplayName(SC_HANDLE, LPCTSTR, LPTSTR, LPDWORD);

SC_HANDLE OpenSCManager(LPCTSTR, LPCTSTR, DWORD);

SC_HANDLE OpenService(SC_HANDLE, LPCTSTR, DWORD);

BOOL QueryServiceConfig(SC_HANDLE, LPQUERY_SERVICE_CONFIG, DWORD, LPDWORD);

BOOL QueryServiceConfig2(SC_HANDLE, DWORD, LPBYTE, DWORD, LPDWORD);

BOOL QueryServiceLockStatus(SC_HANDLE, LPQUERY_SERVICE_LOCK_STATUS, DWORD,
                            LPDWORD);

SERVICE_STATUS_HANDLE RegisterServiceCtrlHandler(LPCTSTR, LPHANDLER_FUNCTION);

SERVICE_STATUS_HANDLE RegisterServiceCtrlHandlerEx(LPCTSTR,
                                                   LPHANDLER_FUNCTION_EX,
                                                   LPVOID);

BOOL StartServiceCtrlDispatcher(const SERVICE_TABLE_ENTRY*);

BOOL StartService(SC_HANDLE, DWORD, LPCTSTR*);

DWORD NotifyServiceStatusChange(SC_HANDLE, DWORD, PSERVICE_NOTIFY);

BOOL ControlServiceEx(SC_HANDLE, DWORD, DWORD, PVOID);

HKL ImmInstallIME(LPCTSTR, LPCTSTR);

UINT ImmGetDescription(HKL, LPTSTR, UINT);

UINT ImmGetIMEFileName(HKL, LPTSTR, UINT);

LONG ImmGetCompositionString(HIMC, DWORD, LPVOID, DWORD);

BOOL ImmSetCompositionString(HIMC, DWORD, LPVOID, DWORD, LPVOID, DWORD);

DWORD ImmGetCandidateListCount(HIMC, LPDWORD);

DWORD ImmGetCandidateList(HIMC, DWORD, LPCANDIDATELIST, DWORD);

DWORD ImmGetGuideLine(HIMC, DWORD, LPTSTR, DWORD);

BOOL ImmGetCompositionFont(HIMC, LPLOGFONT);

BOOL ImmSetCompositionFont(HIMC, LPLOGFONT);

BOOL ImmConfigureIME(HKL, HWND, DWORD, LPVOID);

LRESULT ImmEscape(HKL, HIMC, UINT, LPVOID);

DWORD ImmGetConversionList(HKL, HIMC, LPCTSTR, LPCANDIDATELIST, DWORD, UINT);

BOOL ImmIsUIMessage(HWND, UINT, WPARAM, LPARAM);

BOOL ImmRegisterWord(HKL, LPCTSTR, DWORD, LPCTSTR);

BOOL ImmUnregisterWord(HKL, LPCTSTR, DWORD, LPCTSTR);

UINT ImmGetRegisterWordStyle(HKL, UINT, LPSTYLEBUF);

UINT ImmEnumRegisterWord(HKL, REGISTERWORDENUMPROC, LPCTSTR, DWORD, LPCTSTR,
                         LPVOID);

DWORD ImmGetImeMenuItems(HIMC, DWORD, DWORD, LPIMEMENUITEMINFO,
                         LPIMEMENUITEMINFO, DWORD);
