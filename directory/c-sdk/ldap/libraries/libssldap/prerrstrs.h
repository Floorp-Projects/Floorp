/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

/*
 * pserrstrs.h - map NSPR errors to strings (used by errormap.c)
 *
 */

/*
 ****************************************************************************
 * On 21-June-2002, the code below this point was copied from the file
 * mozilla/security/nss/cmd/lib/NSPRerrs.h (tag NSS_3_4_2_RTM).
 * One addition was made: PR_OPERATION_ABORTED_ERROR.
 ****************************************************************************
 */

/* General NSPR 2.0 errors */
/* Caller must #include "prerror.h" */

ER2( PR_OUT_OF_MEMORY_ERROR, 	"Memory allocation attempt failed." )
ER2( PR_BAD_DESCRIPTOR_ERROR, 	"Invalid file descriptor." )
ER2( PR_WOULD_BLOCK_ERROR, 	"The operation would have blocked." )
ER2( PR_ACCESS_FAULT_ERROR, 	"Invalid memory address argument." )
ER2( PR_INVALID_METHOD_ERROR, 	"Invalid function for file type." )
ER2( PR_ILLEGAL_ACCESS_ERROR, 	"Invalid memory address argument." )
ER2( PR_UNKNOWN_ERROR, 		"Some unknown error has occurred." )
ER2( PR_PENDING_INTERRUPT_ERROR,"Operation interrupted by another thread." )
ER2( PR_NOT_IMPLEMENTED_ERROR, 	"function not implemented." )
ER2( PR_IO_ERROR, 		"I/O function error." )
ER2( PR_IO_TIMEOUT_ERROR, 	"I/O operation timed out." )
ER2( PR_IO_PENDING_ERROR, 	"I/O operation on busy file descriptor." )
ER2( PR_DIRECTORY_OPEN_ERROR, 	"The directory could not be opened." )
ER2( PR_INVALID_ARGUMENT_ERROR, "Invalid function argument." )
ER2( PR_ADDRESS_NOT_AVAILABLE_ERROR, "Network address not available (in use?)." )
ER2( PR_ADDRESS_NOT_SUPPORTED_ERROR, "Network address type not supported." )
ER2( PR_IS_CONNECTED_ERROR, 	"Already connected." )
ER2( PR_BAD_ADDRESS_ERROR, 	"Network address is invalid." )
ER2( PR_ADDRESS_IN_USE_ERROR, 	"Local Network address is in use." )
ER2( PR_CONNECT_REFUSED_ERROR, 	"Connection refused by peer." )
ER2( PR_NETWORK_UNREACHABLE_ERROR, "Network address is presently unreachable." )
ER2( PR_CONNECT_TIMEOUT_ERROR, 	"Connection attempt timed out." )
ER2( PR_NOT_CONNECTED_ERROR, 	"Network file descriptor is not connected." )
ER2( PR_LOAD_LIBRARY_ERROR, 	"Failure to load dynamic library." )
ER2( PR_UNLOAD_LIBRARY_ERROR, 	"Failure to unload dynamic library." )
ER2( PR_FIND_SYMBOL_ERROR, 	
"Symbol not found in any of the loaded dynamic libraries." )
ER2( PR_INSUFFICIENT_RESOURCES_ERROR, "Insufficient system resources." )
ER2( PR_DIRECTORY_LOOKUP_ERROR, 	
"A directory lookup on a network address has failed." )
ER2( PR_TPD_RANGE_ERROR, 		
"Attempt to access a TPD key that is out of range." )
ER2( PR_PROC_DESC_TABLE_FULL_ERROR, "Process open FD table is full." )
ER2( PR_SYS_DESC_TABLE_FULL_ERROR, "System open FD table is full." )
ER2( PR_NOT_SOCKET_ERROR, 	
"Network operation attempted on non-network file descriptor." )
ER2( PR_NOT_TCP_SOCKET_ERROR, 	
"TCP-specific function attempted on a non-TCP file descriptor." )
ER2( PR_SOCKET_ADDRESS_IS_BOUND_ERROR, "TCP file descriptor is already bound." )
ER2( PR_NO_ACCESS_RIGHTS_ERROR, "Access Denied." )
ER2( PR_OPERATION_NOT_SUPPORTED_ERROR, 
"The requested operation is not supported by the platform." )
ER2( PR_PROTOCOL_NOT_SUPPORTED_ERROR, 
"The host operating system does not support the protocol requested." )
ER2( PR_REMOTE_FILE_ERROR, 	"Access to the remote file has been severed." )
ER2( PR_BUFFER_OVERFLOW_ERROR, 	
"The value requested is too large to be stored in the data buffer provided." )
ER2( PR_CONNECT_RESET_ERROR, 	"TCP connection reset by peer." )
ER2( PR_RANGE_ERROR, 		"Unused." )
ER2( PR_DEADLOCK_ERROR, 	"The operation would have deadlocked." )
ER2( PR_FILE_IS_LOCKED_ERROR, 	"The file is already locked." )
ER2( PR_FILE_TOO_BIG_ERROR, 	
"Write would result in file larger than the system allows." )
ER2( PR_NO_DEVICE_SPACE_ERROR, 	"The device for storing the file is full." )
ER2( PR_PIPE_ERROR, 		"Unused." )
ER2( PR_NO_SEEK_DEVICE_ERROR, 	"Unused." )
ER2( PR_IS_DIRECTORY_ERROR, 	
"Cannot perform a normal file operation on a directory." )
ER2( PR_LOOP_ERROR, 		"Symbolic link loop." )
ER2( PR_NAME_TOO_LONG_ERROR, 	"File name is too long." )
ER2( PR_FILE_NOT_FOUND_ERROR, 	"File not found." )
ER2( PR_NOT_DIRECTORY_ERROR, 	
"Cannot perform directory operation on a normal file." )
ER2( PR_READ_ONLY_FILESYSTEM_ERROR, 
"Cannot write to a read-only file system." )
ER2( PR_DIRECTORY_NOT_EMPTY_ERROR, 
"Cannot delete a directory that is not empty." )
ER2( PR_FILESYSTEM_MOUNTED_ERROR, 
"Cannot delete or rename a file object while the file system is busy." )
ER2( PR_NOT_SAME_DEVICE_ERROR, 	
"Cannot rename a file to a file system on another device." )
ER2( PR_DIRECTORY_CORRUPTED_ERROR, 
"The directory object in the file system is corrupted." )
ER2( PR_FILE_EXISTS_ERROR, 	
"Cannot create or rename a filename that already exists." )
ER2( PR_MAX_DIRECTORY_ENTRIES_ERROR, 
"Directory is full.  No additional filenames may be added." )
ER2( PR_INVALID_DEVICE_STATE_ERROR, 
"The required device was in an invalid state." )
ER2( PR_DEVICE_IS_LOCKED_ERROR, "The device is locked." )
ER2( PR_NO_MORE_FILES_ERROR, 	"No more entries in the directory." )
ER2( PR_END_OF_FILE_ERROR, 	"Encountered end of file." )
ER2( PR_FILE_SEEK_ERROR, 	"Seek error." )
ER2( PR_FILE_IS_BUSY_ERROR, 	"The file is busy." )
ER2( PR_OPERATION_ABORTED_ERROR,  "The I/O operation was aborted" )
ER2( PR_IN_PROGRESS_ERROR,
"Operation is still in progress (probably a non-blocking connect)." )
ER2( PR_ALREADY_INITIATED_ERROR,
"Operation has already been initiated (probably a non-blocking connect)." )

#ifdef PR_GROUP_EMPTY_ERROR
ER2( PR_GROUP_EMPTY_ERROR, 	"The wait group is empty." )
#endif

#ifdef PR_INVALID_STATE_ERROR
ER2( PR_INVALID_STATE_ERROR, 	"Object state improper for request." )
#endif

#ifdef PR_NETWORK_DOWN_ERROR
ER2( PR_NETWORK_DOWN_ERROR,	"Network is down." )
#endif

#ifdef PR_SOCKET_SHUTDOWN_ERROR
ER2( PR_SOCKET_SHUTDOWN_ERROR,	"The socket was previously shut down." )
#endif

#ifdef PR_CONNECT_ABORTED_ERROR
ER2( PR_CONNECT_ABORTED_ERROR,	"TCP Connection aborted." )
#endif

#ifdef PR_HOST_UNREACHABLE_ERROR
ER2( PR_HOST_UNREACHABLE_ERROR,	"Host is unreachable." )
#endif

/* always last */
ER2( PR_MAX_ERROR, 		"Placeholder for the end of the list" )
