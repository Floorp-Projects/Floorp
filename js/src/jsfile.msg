/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
        Error messages for JSFile. See js.msg for format.
*/

MSG_DEF(JSFILEMSG_NOT_AN_ERROR,                                    0, 0, JSEXN_NONE, "<Error #0 is reserved>")
MSG_DEF(JSFILEMSG_FILE_CONSTRUCTOR_UNDEFINED_ERROR,                1, 0, JSEXN_NONE, "File constructor is undefined")
MSG_DEF(JSFILEMSG_FILE_CURRENTDIR_UNDEFINED_ERROR,                 2, 0, JSEXN_NONE, "File.currentDir is undefined")
MSG_DEF(JSFILEMSG_FIRST_ARGUMENT_OPEN_NOT_STRING_ERROR,            3, 1, JSEXN_NONE, "The first argument {0} to file.open must be a string")
MSG_DEF(JSFILEMSG_SECOND_ARGUMENT_OPEN_NOT_STRING_ERROR,           4, 0, JSEXN_NONE, "The second argument to file.open must be a string")
MSG_DEF(JSFILEMSG_CANNOT_COPY_FILE_OPEN_FOR_WRITING_ERROR,         5, 1, JSEXN_NONE, "Cannot copy file {0} open for writing")
MSG_DEF(JSFILEMSG_CANNOT_ACCESS_FILE_INFO_ERROR,                   6, 1, JSEXN_NONE, "Cannot access file information for {0}")
MSG_DEF(JSFILEMSG_COPY_READ_ERROR,                                 7, 1, JSEXN_NONE, "An error occured while attempting to read a file {0} to copy")
MSG_DEF(JSFILEMSG_COPY_WRITE_ERROR,                                8, 1, JSEXN_NONE, "An error occured while attempting to copy into file {0}")
MSG_DEF(JSFILEMSG_RENAME_EXPECTS_ONE_ARG_ERROR,                    9, 0, JSEXN_NONE, "file.renameTo expects one argument")
MSG_DEF(JSFILEMSG_CANNOT_FLUSH_CLOSE_FILE_ERROR,                  10, 1, JSEXN_NONE, "Cannot flush closed file {0}")
MSG_DEF(JSFILEMSG_CANNOT_OPEN_WRITING_ERROR,                      11, 1, JSEXN_NONE, "Cannot open file {0} for writing")
MSG_DEF(JSFILEMSG_WRITEALL_EXPECTS_ONE_ARG_ERROR,                 12, 0, JSEXN_NONE, "writeAll expects one argument")
MSG_DEF(JSFILEMSG_FIRST_ARGUMENT_WRITEALL_NOT_ARRAY_ERROR,        13, 0, JSEXN_NONE, "writeAll expects an array as an argument")
MSG_DEF(JSFILEMSG_CANNOT_OPEN_FILE_ERROR,                         15, 1, JSEXN_NONE, "Cannot open file {0}")
MSG_DEF(JSFILEMSG_FIRST_ARGUMENT_CONSTRUCTOR_NOT_STRING_ERROR,    16, 1, JSEXN_NONE, "The argument to the File constructor {0} must be a string")
MSG_DEF(JSFILEMSG_BIDIRECTIONAL_PIPE_NOT_SUPPORTED,				  17, 0, JSEXN_NONE, "Bidirectional pipes are not supported")
MSG_DEF(JSFILEMSG_OPEN_MODE_NOT_SUPPORTED_WITH_PIPES,		      18, 2, JSEXN_NONE, "The opening mode you have chosen {0} is not supported by the pipe you are trying to open: {1}")

