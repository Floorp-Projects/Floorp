/*
  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright (c) 2005-2013 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:
  http://software.intel.com/en-us/articles/intel-vtune-amplifier-xe/

  BSD LICENSE

  Copyright (c) 2005-2013 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __JITPROFILING_H__
#define __JITPROFILING_H__

/**
 * @brief JIT Profiling APIs
 *
 * The JIT Profiling API is used to report information about just-in-time
 * generated code that can be used by performance tools. The user inserts
 * calls in the code generator to report information before JIT-compiled
 * code goes to execution. This information is collected at runtime and used
 * by tools like Intel(R) VTune(TM) Amplifier to display performance metrics
 * associated with JIT-compiled code.
 *
 * These APIs can be used to\n
 * **Profile trace-based and method-based JIT-compiled
 * code**. Some examples of environments that you can profile with this APIs:
 * dynamic JIT compilation of JavaScript code traces, OpenCL JIT execution,
 * Java/.NET managed execution environments, and custom ISV JIT engines.
 *
 * @code
 * #include <jitprofiling.h>
 *
 * if (iJIT_IsProfilingActive != iJIT_SAMPLING_ON) {
 *     return;
 * }
 *
 * iJIT_Method_Load jmethod = {0};
 * jmethod.method_id = iJIT_GetNewMethodID();
 * jmethod.method_name = "method_name";
 * jmethod.class_file_name = "class_name";
 * jmethod.source_file_name = "source_file_name";
 * jmethod.method_load_address = code_addr;
 * jmethod.method_size = code_size;
 *
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&jmethod);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_SHUTDOWN, NULL);
 * @endcode
 *
 *  * Expected behaviour:
 *    * If any iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED event overwrites
 *      already reported method, then such a method becomes invalid and its
 *      memory region is treated as unloaded. VTune Amplifier displays the metrics
 *      collected by the method until it is overwritten.
 *    * If supplied line number information contains multiple source lines for
 *      the same assembly instruction (code location), then VTune Amplifier picks up
 *      the first line number.
 *    * Dynamically generated code can be associated with a module name.
 *      Use the iJIT_Method_Load_V2 structure.\n
 *      Clarification of some cases:\n
 *        * If you register a function with the same method ID multiple times
 *          specifying different module names, then the VTune Amplifier picks up
 *          the module name registered first. If you want to distinguish the same
 *          function between different JIT engines, supply different method IDs for
 *          each function. Other symbolic information (for example, source file)
 *          can be identical.
 *
 * **Analyze split functions** (multiple joint or disjoint code regions
 * belonging to the same function) **including re-JIT**
 * with potential overlapping of code regions in time, which is common in
 * resource-limited environments.
 * @code
 * #include <jitprofiling.h>
 *
 * unsigned int method_id = iJIT_GetNewMethodID();
 *
 * iJIT_Method_Load a = {0};
 * a.method_id = method_id;
 * a.method_load_address = acode_addr;
 * a.method_size = acode_size;
 *
 * iJIT_Method_Load b = {0};
 * b.method_id = method_id;
 * b.method_load_address = baddr_second;
 * b.method_size = bsize_second;
 *
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&a);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&b);
 * @endcode
 *
 *  * Expected behaviour:
 *      * If a iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED event overwrites
 *        already reported method, then such a method becomes invalid and
 *        its memory region is treated as unloaded.
 *      * All code regions reported with the same method ID are considered as
 *        belonging to the same method. Symbolic information (method name,
 *        source file name) will be taken from the first notification, all
 *        subsequent notifications with the same method ID will be processed
 *        only for line number table information. So, the VTune Amplifier will map
 *        samples to a source line using the line number table from the current
 *        notification while taking source file name from the very first one.\n
 *        Clarification of some cases:\n
 *          * If you register a second code region with a different source file
 *          name and the same method ID, then this information will be saved and
 *          will not be considered as an extension of the first code region, but
 *          VTune Amplifier will use source file of the first code region and map
 *          performance metrics incorrectly.
 *          * If you register a second code region with the same source file as
 *          for the first region and the same method ID, then source file will be
 *          discarded but VTune Amplifier will map metrics to the source file correctly.
 *          * If you register a second code region with a null source file and
 *          the same method ID, then provided line number info will be associated
 *          with the source file of the first code region.
 *
 * **Explore inline functions** including multi-level hierarchy of
 * nested inlines to see how performance metrics are distributed through them.
 *
 * @code
 * #include <jitprofiling.h>
 *
 *  //                                    method_id   parent_id
 *  //   [-- c --]                          3000        2000
 *  //                  [---- d -----]      2001        1000
 *  //  [---- b ----]                       2000        1000
 *  // [------------ a ----------------]    1000         n/a
 *
 * iJIT_Method_Load a = {0};
 * a.method_id = 1000;
 *
 * iJIT_Method_Inline_Load b = {0};
 * b.method_id = 2000;
 * b.parent_method_id = 1000;
 *
 * iJIT_Method_Inline_Load c = {0};
 * c.method_id = 3000;
 * c.parent_method_id = 2000;
 *
 * iJIT_Method_Inline_Load d = {0};
 * d.method_id = 2001;
 * d.parent_method_id = 1000;
 *
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&a);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED, (void*)&b);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED, (void*)&c);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED, (void*)&d);
 * @endcode
 *
 *  * Requirements:
 *      * Each inline (iJIT_Method_Inline_Load) method should be associated
 *        with two method IDs: one for itself, one for its immediate parent.
 *      * Address regions of inline methods of the same parent method cannot
 *        overlap each other.
 *      * Execution of the parent method must not be started until it and all
 *        its inlines are reported.
 *  * Expected behaviour:
 *      * In case of nested inlines an order of
 *        iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED events is not important.
 *      * If any event overwrites either inline method or top parent method,
 *        then the parent including inlines becomes invalid and its memory
 *        region is treated as unloaded.
 *
 * **Life time of allocated data**\n
 * The client sends an event notification to the agent with event-specific
 * data, which is a structure. The pointers in the structure refers to memory
 * allocated by the client, which responsible for releasing it. The pointers are
 * used by the iJIT_NotifyEvent method to copy client's data in a trace file
 * and they are not used after the iJIT_NotifyEvent method returns.
 *
 */

/**
 * @defgroup jitapi JIT Profiling
 * @ingroup internal
 * @{
 */

/**
 * @enum iJIT_jvm_event
 * @brief Enumerator for the types of notifications
 */
typedef enum iJIT_jvm_event
{
    iJVM_EVENT_TYPE_SHUTDOWN = 2,               /**< Send to shutdown the agent.
                                                 * Use NULL for event data. */

    iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED = 13,  /**< Send when a dynamic code is
                                                 * JIT compiled and loaded into
                                                 * memory by the JIT engine but
                                                 * before the code is executed.
                                                 * Use iJIT_Method_Load as event
                                                 * data. */
/** @cond exclude_from_documentation */
    iJVM_EVENT_TYPE_METHOD_UNLOAD_START,    /**< Send when a compiled dynamic
                                             * code is being unloaded from memory.
                                             * Use iJIT_Method_Load as event data.*/
/** @endcond */

//TODO: add a note that line info assumes from method load
    iJVM_EVENT_TYPE_METHOD_UPDATE,   /**< Send to provide a new content for
                                      * an early reported dynamic code.
                                      * The previous content will be invalidate
                                      * starting from time of the notification.
                                      * Use iJIT_Method_Load as event data but
                                      * required fields are following:
                                      * - method_id    identify the code to update.
                                      * - method_load_address    specify start address
                                      *                          within identified code range
                                      *                          where update should be started.
                                      * - method_size            specify length of updated code
                                      *                          range. */

    iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED, /**< Send when an inline dynamic
                                                  * code is JIT compiled and loaded
                                                  * into memory by the JIT engine
                                                  * but before the parent code region
                                                  * is started executing.
                                                  * Use iJIT_Method_Inline_Load as event data.*/

/** @cond exclude_from_documentation */
    /* Legacy stuff. Do not use it. */
    iJVM_EVENT_TYPE_ENTER_NIDS = 19,
    iJVM_EVENT_TYPE_LEAVE_NIDS,
/** @endcond */

    iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2  /**< Send when a dynamic code is
                                              * JIT compiled and loaded into
                                              * memory by the JIT engine but
                                              * before the code is executed.
                                              * Use iJIT_Method_Load_V2 as event
                                              * data. */
} iJIT_JVM_EVENT;

/** @cond exclude_from_documentation */
/* Legacy stuff. Do not use it. */
typedef enum _iJIT_ModeFlags
{
    iJIT_NO_NOTIFICATIONS          = 0x0000,
    iJIT_BE_NOTIFY_ON_LOAD         = 0x0001,
    iJIT_BE_NOTIFY_ON_UNLOAD       = 0x0002,
    iJIT_BE_NOTIFY_ON_METHOD_ENTRY = 0x0004,
    iJIT_BE_NOTIFY_ON_METHOD_EXIT  = 0x0008

} iJIT_ModeFlags;
/** @endcond */

/**
 * @enum _iJIT_IsProfilingActiveFlags
 * @brief Enumerator for the agent's mode
 */
typedef enum _iJIT_IsProfilingActiveFlags
{
    iJIT_NOTHING_RUNNING           = 0x0000,    /**< The agent is not running.
                                                 * iJIT_NotifyEvent calls will
                                                 * not be processed. */
    iJIT_SAMPLING_ON               = 0x0001,    /**< The agent is running and
                                                 * ready to process notifications. */

/** @cond exclude_from_documentation */
    /* Legacy. Call Graph is running */
    iJIT_CALLGRAPH_ON              = 0x0002
/** @endcond */

} iJIT_IsProfilingActiveFlags;

/** @cond exclude_from_documentation */
/* Legacy stuff. Do not use it. */
typedef enum _iJDEnvironmentType
{
    iJDE_JittingAPI = 2

} iJDEnvironmentType;

typedef struct _iJIT_Method_Id
{
    unsigned int method_id;

} *piJIT_Method_Id, iJIT_Method_Id;

typedef struct _iJIT_Method_NIDS
{
    unsigned int method_id;     /**< Unique method ID */
    unsigned int stack_id;      /**< NOTE: no need to fill this field,
                                 * it's filled by VTune Amplifier */
    char*  method_name;         /**< Method name (just the method, without the class) */

} *piJIT_Method_NIDS, iJIT_Method_NIDS;
/** @endcond */

typedef enum _iJIT_CodeType
{
    iJIT_CT_UNKNOWN = 0,
    iJIT_CT_CODE,             // executable code
    iJIT_CT_DATA,             // this kind of “update” will be excluded from the function’s body.
    iJIT_CT_EOF
} iJIT_CodeType;

typedef struct _iJIT_Method_Update
{
    unsigned int method_id;
    void* load_address;
    unsigned int size;
    iJIT_CodeType type;

} *piJIT_Method_Update, iJIT_Method_Update;

/**
 * @details Describes a single entry in the line number information of
 * a code region that gives information about how the reported code region
 * is mapped to source file.
 * Intel(R) VTune(TM) Amplifier uses line number information to attribute
 * the samples (virtual address) to a line number. \n
 * It is acceptable to report different code addresses for the same source line:
 * @code
 *   Offset LineNumber
 *      1       2
 *      12      4
 *      15      2
 *      18      1
 *      21      30
 *
 *  VTune(TM) Amplifier XE contsructs the following table using the client data
 *
 *   Code subrange  Line number
 *      0-1             2
 *      1-12            4
 *      12-15           2
 *      15-18           1
 *      18-21           30
 * @endcode
 */
typedef struct _LineNumberInfo
{
    unsigned int Offset;     /**< Offset from the begining of the code region. */
    unsigned int LineNumber; /**< Matching source line number offset (from beginning of source file). */

} *pLineNumberInfo, LineNumberInfo;

/**
 *  Description of a JIT-compiled method
 */
typedef struct _iJIT_Method_Load
{
    unsigned int method_id; /**< Unique method ID.
                             *  Method ID cannot be smaller than 999.
                             *  Either you use the API function
                             *  iJIT_GetNewMethodID to get a valid and unique
                             *  method ID, or you take care of ID uniqueness
                             *  and correct range by yourself.\n
                             *  You must use the same method ID for all code
                             *  regions of the same method, otherwise different
                             *  method IDs mean different methods. */

    char* method_name; /** The name of the method. It can be optionally
                        *  prefixed with its class name and appended with
                        *  its complete signature. Can't be  NULL. */

    void* method_load_address; /** The start virtual address of the method code
                                *  region. If NULL that data provided with
                                *  event are not accepted. */

    unsigned int method_size; /** The code size of the method in memory.
                               *  If 0, then data provided with the event are not
                               *  accepted. */

    unsigned int line_number_size; /** The number of entries in the line number
                                    *  table.0 if none. */

    pLineNumberInfo line_number_table; /** Pointer to the line numbers info
                                        *  array. Can be NULL if
                                        *  line_number_size is 0. See
                                        *  LineNumberInfo Structure for a
                                        *  description of a single entry in
                                        *  the line number info array */

    unsigned int class_id; /** This field is obsolete. */

    char* class_file_name; /** Class name. Can be NULL.*/

    char* source_file_name; /** Source file name. Can be NULL.*/

    void* user_data; /** This field is obsolete. */

    unsigned int user_data_size; /** This field is obsolete. */

    iJDEnvironmentType  env; /** This field is obsolete. */

} *piJIT_Method_Load, iJIT_Method_Load;

#pragma pack(push, 8)
/**
 *  Description of a JIT-compiled method
 *
 *  When you use the iJIT_Method_Load_V2 structure to describe
 *  the JIT compiled method, use iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2
 *  as an event type to report it.
 */
typedef struct _iJIT_Method_Load_V2
{
    unsigned int method_id; /**< Unique method ID.
                             *  Method ID cannot be smaller than 999.
                             *  Either you use the API function
                             *  iJIT_GetNewMethodID to get a valid and unique
                             *  method ID, or you take care of ID uniqueness
                             *  and correct range by yourself.\n
                             *  You must use the same method ID for all code
                             *  regions of the same method, otherwise different
                             *  method IDs mean different methods. */

    char* method_name; /** The name of the method. It can be optionally
                        *  prefixed with its class name and appended with
                        *  its complete signature. Can't be  NULL. */

    void* method_load_address; /** The start virtual address of the method code
                                *  region. If NULL that data provided with
                                *  event are not accepted. */

    unsigned int method_size; /** The code size of the method in memory.
                               *  If 0, then data provided with the event are not
                               *  accepted. */

    unsigned int line_number_size; /** The number of entries in the line number
                                    *  table.0 if none. */

    pLineNumberInfo line_number_table; /** Pointer to the line numbers info
                                        *  array. Can be NULL if
                                        *  line_number_size is 0. See
                                        *  LineNumberInfo Structure for a
                                        *  description of a single entry in
                                        *  the line number info array */

    char* class_file_name; /** Class name. Can be NULL.*/

    char* source_file_name; /** Source file name. Can be NULL.*/

    char* module_name; /** Module name. Can be NULL.
                           The module name can be useful for distinguishing among
                           different JIT engines. Intel VTune Amplifier will display
                           reported methods split by specified modules */

} *piJIT_Method_Load_V2, iJIT_Method_Load_V2;
#pragma pack(pop)

/**
 * Description of an inline JIT-compiled method
 */
typedef struct _iJIT_Method_Inline_Load
{
    unsigned int method_id; /**< Unique method ID.
                             *  Method ID cannot be smaller than 999.
                             *  Either you use the API function
                             *  iJIT_GetNewMethodID to get a valid and unique
                             *  method ID, or you take care of ID uniqueness
                             *  and correct range by yourself. */

    unsigned int parent_method_id; /** Unique immediate parent's method ID.
                                    *  Method ID may not be smaller than 999.
                                    *  Either you use the API function
                                    *  iJIT_GetNewMethodID to get a valid and unique
                                    *  method ID, or you take care of ID uniqueness
                                    *  and correct range by yourself. */

    char* method_name; /** The name of the method. It can be optionally
                        *  prefixed with its class name and appended with
                        *  its complete signature. Can't be  NULL. */

    void* method_load_address;  /** The virtual address on which the method
                                  * is inlined. If NULL, then data provided with
                                  * the event are not accepted. */

    unsigned int method_size; /** The code size of the method in memory.
                               *  If 0 that data provided with event are not
                               *  accepted. */

    unsigned int line_number_size; /** The number of entries in the line number
                                    *  table. 0 if none. */

    pLineNumberInfo line_number_table; /** Pointer to the line numbers info
                                        *  array. Can be NULL if
                                        *  line_number_size is 0. See
                                        *  LineNumberInfo Structure for a
                                        *  description of a single entry in
                                        *  the line number info array */

    char* class_file_name; /** Class name. Can be NULL.*/

    char* source_file_name; /** Source file name. Can be NULL.*/

} *piJIT_Method_Inline_Load, iJIT_Method_Inline_Load;

/** @cond exclude_from_documentation */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CDECL
#  if defined WIN32 || defined _WIN32
#    define CDECL __cdecl
#  else /* defined WIN32 || defined _WIN32 */
#    if defined _M_X64 || defined _M_AMD64 || defined __x86_64__
#      define CDECL /* not actual on x86_64 platform */
#    else  /* _M_X64 || _M_AMD64 || __x86_64__ */
#      define CDECL __attribute__ ((cdecl))
#    endif /* _M_X64 || _M_AMD64 || __x86_64__ */
#  endif /* defined WIN32 || defined _WIN32 */
#endif /* CDECL */

#define JITAPI CDECL
/** @endcond */

/**
 * @brief Generates a new unique method ID.
 *
 * You must use this API to obtain unique and valid method IDs for methods or
 * traces reported to the agent if you don't have you own mechanism to generate
 * unique method IDs.
 *
 * @return a new unique method ID. When out of unique method IDs, this API
 * returns 0, which is not an accepted value.
 */
unsigned int JITAPI iJIT_GetNewMethodID(void);

/**
 * @brief Returns the current mode of the agent.
 *
 * @return iJIT_SAMPLING_ON, indicating that agent is running, or
 * iJIT_NOTHING_RUNNING if no agent is running.
 */
iJIT_IsProfilingActiveFlags JITAPI iJIT_IsProfilingActive(void);

/**
 * @brief Reports infomation about JIT-compiled code to the agent.
 *
 * The reported information is used to attribute samples obtained from any
 * Intel(R) VTune(TM) Amplifier collector. This API needs to be called
 * after JIT compilation and before the first entry into the JIT compiled
 * code.
 *
 * @param[in] event_type - type of the data sent to the agent
 * @param[in] EventSpecificData - pointer to event-specific data
 *
 * @returns 1 on success, otherwise 0.
 */
int JITAPI iJIT_NotifyEvent(iJIT_JVM_EVENT event_type, void* EventSpecificData);

/** @cond exclude_from_documentation */
/*
 * Do not use these legacy APIs, which are here for backward compatibility
 * with Intel(R) VTune(TM) Performance Analyzer.
 */
typedef void (*iJIT_ModeChangedEx)(void* UserData, iJIT_ModeFlags Flags);
void JITAPI iJIT_RegisterCallbackEx(void* userdata,
                                    iJIT_ModeChangedEx NewModeCallBackFuncEx);
void JITAPI FinalizeThread(void);
void JITAPI FinalizeProcess(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
/** @endcond */

/** @} jitapi group */

#endif /* __JITPROFILING_H__ */
