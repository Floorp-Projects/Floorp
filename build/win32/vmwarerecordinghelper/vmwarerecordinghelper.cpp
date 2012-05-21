/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The following code comes from "Starting and Stopping Recording of Virtual
 * Machine Activity from Within the Guest":
 *
 *   http://kb.vmware.com/selfservice/documentLink.do?externalID=1001401
 */

void __cdecl
StartRecording()
{
  __asm {
    mov eax, 564d5868h
    mov ebx, 1
    mov cx, 47
    mov dx, 5658h
    in eax, dx
  }
}

void __cdecl
StopRecording()
{
  __asm {
    mov eax, 564d5868h
    mov ebx, 2
    mov cx, 47
    mov dx, 5658h
    in eax, dx 
  }
}
