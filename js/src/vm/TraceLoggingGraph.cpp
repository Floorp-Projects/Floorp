/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TraceLoggingGraph.h"

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include "mozilla/Endian.h"

#include "jsstr.h"

#include "threading/LockGuard.h"
#include "vm/TraceLogging.h"

#ifndef TRACE_LOG_DIR
# if defined(_WIN32)
#  define TRACE_LOG_DIR ""
# else
#  define TRACE_LOG_DIR "/tmp/"
# endif
#endif

using mozilla::NativeEndian;

TraceLoggerGraphState* traceLoggerGraphState = nullptr;

bool
TraceLoggerGraphState::init()
{
    pid_ = (uint32_t) getpid();

    char filename[sizeof TRACE_LOG_DIR "tl-data.4294967295.json"];
    sprintf(filename, TRACE_LOG_DIR "tl-data.%u.json", pid_);
    out = fopen(filename, "w");
    if (!out)
        return false;

    fprintf(out, "[");

    // Write the last tl-data.*.json file to tl-data.json.
    // In most cases that is the wanted file.
    if (FILE* last = fopen(TRACE_LOG_DIR "tl-data.json", "w")) {
        fprintf(last, "\"tl-data.%u.json\"", pid_);
        fclose(last);
    }

#ifdef DEBUG
    initialized = true;
#endif
    return true;
}

TraceLoggerGraphState::~TraceLoggerGraphState()
{
    if (out) {
        fprintf(out, "]");
        fclose(out);
        out = nullptr;
    }

#ifdef DEBUG
    initialized = false;
#endif
}

uint32_t
TraceLoggerGraphState::nextLoggerId()
{
    js::LockGuard<js::Mutex> guard(lock);

    MOZ_ASSERT(initialized);

    if (numLoggers > 999) {
        fprintf(stderr, "TraceLogging: Can't create more than 999 different loggers.");
        return uint32_t(-1);
    }

    if (numLoggers > 0) {
        int written = fprintf(out, ",\n");
        if (written < 0) {
            fprintf(stderr, "TraceLogging: Error while writing.\n");
            return uint32_t(-1);
        }
    }

    int written = fprintf(out, "{\"tree\":\"tl-tree.%u.%d.tl\", \"events\":\"tl-event.%u.%d.tl\", "
                               "\"dict\":\"tl-dict.%u.%d.json\", \"treeFormat\":\"64,64,31,1,32\"}",
                          pid_, numLoggers, pid_, numLoggers, pid_, numLoggers);
    if (written < 0) {
        fprintf(stderr, "TraceLogging: Error while writing.\n");
        return uint32_t(-1);
    }

    return numLoggers++;
}

static bool
EnsureTraceLoggerGraphState()
{
    if (MOZ_LIKELY(traceLoggerGraphState))
        return true;

    traceLoggerGraphState = js_new<TraceLoggerGraphState>();
    if (!traceLoggerGraphState)
        return false;

    if (!traceLoggerGraphState->init()) {
        js::DestroyTraceLoggerGraphState();
        return false;
    }

    return true;
}

void
js::DestroyTraceLoggerGraphState()
{
    if (traceLoggerGraphState) {
        js_delete(traceLoggerGraphState);
        traceLoggerGraphState = nullptr;
    }
}

bool
TraceLoggerGraph::init(uint64_t startTimestamp)
{
    if (!tree.init()) {
        failed = true;
        return false;
    }
    if (!stack.init()) {
        failed = true;
        return false;
    }

    if (!EnsureTraceLoggerGraphState()) {
        failed = true;
        return false;
    }

    uint32_t loggerId = traceLoggerGraphState->nextLoggerId();
    if (loggerId == uint32_t(-1)) {
        failed = true;
        return false;
    }

    uint32_t pid = traceLoggerGraphState->pid();

    char dictFilename[sizeof TRACE_LOG_DIR "tl-dict.4294967295.100.json"];
    sprintf(dictFilename, TRACE_LOG_DIR "tl-dict.%u.%d.json", pid, loggerId);
    dictFile = fopen(dictFilename, "w");
    if (!dictFile) {
        failed = true;
        return false;
    }

    char treeFilename[sizeof TRACE_LOG_DIR "tl-tree.4294967295.100.tl"];
    sprintf(treeFilename, TRACE_LOG_DIR "tl-tree.%u.%d.tl", pid, loggerId);
    treeFile = fopen(treeFilename, "w+b");
    if (!treeFile) {
        fclose(dictFile);
        dictFile = nullptr;
        failed = true;
        return false;
    }

    char eventFilename[sizeof TRACE_LOG_DIR "tl-event.4294967295.100.tl"];
    sprintf(eventFilename, TRACE_LOG_DIR "tl-event.%u.%d.tl", pid, loggerId);
    eventFile = fopen(eventFilename, "wb");
    if (!eventFile) {
        fclose(dictFile);
        fclose(treeFile);
        dictFile = nullptr;
        treeFile = nullptr;
        failed = true;
        return false;
    }

    // Create the top tree node and corresponding first stack item.
    TreeEntry& treeEntry = tree.pushUninitialized();
    treeEntry.setStart(startTimestamp);
    treeEntry.setStop(0);
    treeEntry.setTextId(0);
    treeEntry.setHasChildren(false);
    treeEntry.setNextId(0);

    StackEntry& stackEntry = stack.pushUninitialized();
    stackEntry.setTreeId(0);
    stackEntry.setLastChildId(0);
    stackEntry.setActive(true);

    int written = fprintf(dictFile, "[");
    if (written < 0) {
        fprintf(stderr, "TraceLogging: Error while writing.\n");
        fclose(dictFile);
        fclose(treeFile);
        fclose(eventFile);
        dictFile = nullptr;
        treeFile = nullptr;
        eventFile = nullptr;
        failed = true;
        return false;
    }

    return true;
}

TraceLoggerGraph::~TraceLoggerGraph()
{
    // Write dictionary to disk
    if (dictFile) {
        int written = fprintf(dictFile, "]");
        if (written < 0)
            fprintf(stderr, "TraceLogging: Error while writing.\n");
        fclose(dictFile);

        dictFile = nullptr;
    }

    if (!failed && treeFile) {
        // Make sure every start entry has a corresponding stop value.
        // We temporary enable logging for this. Stop doesn't need any extra data,
        // so is safe to do, even when we encountered OOM.
        enabled = 1;
        while (stack.size() > 1)
            stopEvent(0);
        enabled = 0;
    }

    if (!failed && !flush()) {
        fprintf(stderr, "TraceLogging: Couldn't write the data to disk.\n");
        enabled = 0;
        failed = true;
    }

    if (treeFile) {
        fclose(treeFile);
        treeFile = nullptr;
    }

    if (eventFile) {
        fclose(eventFile);
        eventFile = nullptr;
    }
}

bool
TraceLoggerGraph::flush()
{
    MOZ_ASSERT(!failed);

    if (treeFile) {
        // Format data in big endian.
        for (size_t i = 0; i < tree.size(); i++)
            entryToBigEndian(&tree[i]);

        int success = fseek(treeFile, 0, SEEK_END);
        if (success != 0)
            return false;

        size_t bytesWritten = fwrite(tree.data(), sizeof(TreeEntry), tree.size(), treeFile);
        if (bytesWritten < tree.size())
            return false;

        treeOffset += tree.size();
        tree.clear();
    }

    return true;
}

void
TraceLoggerGraph::entryToBigEndian(TreeEntry* entry)
{
    entry->start_ = NativeEndian::swapToBigEndian(entry->start_);
    entry->stop_ = NativeEndian::swapToBigEndian(entry->stop_);
    uint32_t data = (entry->u.s.textId_ << 1) + entry->u.s.hasChildren_;
    entry->u.value_ = NativeEndian::swapToBigEndian(data);
    entry->nextId_ = NativeEndian::swapToBigEndian(entry->nextId_);
}

void
TraceLoggerGraph::entryToSystemEndian(TreeEntry* entry)
{
    entry->start_ = NativeEndian::swapFromBigEndian(entry->start_);
    entry->stop_ = NativeEndian::swapFromBigEndian(entry->stop_);

    uint32_t data = NativeEndian::swapFromBigEndian(entry->u.value_);
    entry->u.s.textId_ = data >> 1;
    entry->u.s.hasChildren_ = data & 0x1;

    entry->nextId_ = NativeEndian::swapFromBigEndian(entry->nextId_);
}

void
TraceLoggerGraph::startEvent(uint32_t id, uint64_t timestamp)
{
    if (failed || enabled == 0)
        return;

    if (!tree.hasSpaceForAdd()) {
        if (tree.size() >= treeSizeFlushLimit() || !tree.ensureSpaceBeforeAdd()) {
            if (!flush()) {
                fprintf(stderr, "TraceLogging: Couldn't write the data to disk.\n");
                enabled = 0;
                failed = true;
                return;
            }
        }
    }

    if (!startEventInternal(id, timestamp)) {
        fprintf(stderr, "TraceLogging: Failed to start an event.\n");
        enabled = 0;
        failed = true;
        return;
    }
}

TraceLoggerGraph::StackEntry&
TraceLoggerGraph::getActiveAncestor()
{
    uint32_t parentId = stack.lastEntryId();
    while (!stack[parentId].active())
        parentId--;
    return stack[parentId];
}

bool
TraceLoggerGraph::startEventInternal(uint32_t id, uint64_t timestamp)
{
    if (!stack.ensureSpaceBeforeAdd())
        return false;

    // Patch up the tree to be correct. There are two scenarios:
    // 1) Parent has no children yet. So update parent to include children.
    // 2) Parent has already children. Update last child to link to the new
    //    child.
    StackEntry& parent = getActiveAncestor();
#ifdef DEBUG
    TreeEntry entry;
    if (!getTreeEntry(parent.treeId(), &entry))
        return false;
#endif

    if (parent.lastChildId() == 0) {
        MOZ_ASSERT(!entry.hasChildren());
        MOZ_ASSERT(parent.treeId() == treeOffset + tree.size() - 1);

        if (!updateHasChildren(parent.treeId()))
            return false;
    } else {
        MOZ_ASSERT(entry.hasChildren());

        if (!updateNextId(parent.lastChildId(), tree.size() + treeOffset))
            return false;
    }

    // Add a new tree entry.
    TreeEntry& treeEntry = tree.pushUninitialized();
    treeEntry.setStart(timestamp);
    treeEntry.setStop(0);
    treeEntry.setTextId(id);
    treeEntry.setHasChildren(false);
    treeEntry.setNextId(0);

    // Add a new stack entry.
    StackEntry& stackEntry = stack.pushUninitialized();
    stackEntry.setTreeId(tree.lastEntryId() + treeOffset);
    stackEntry.setLastChildId(0);
    stackEntry.setActive(true);

    // Set the last child of the parent to this newly added entry.
    parent.setLastChildId(tree.lastEntryId() + treeOffset);

    return true;
}

void
TraceLoggerGraph::stopEvent(uint32_t id, uint64_t timestamp)
{
#ifdef DEBUG
    if (id != TraceLogger_Scripts &&
        id != TraceLogger_Engine &&
        stack.size() > 1 &&
        stack.lastEntry().active())
    {
        TreeEntry entry;
        MOZ_ASSERT(getTreeEntry(stack.lastEntry().treeId(), &entry));
        MOZ_ASSERT(entry.textId() == id);
    }
#endif

    stopEvent(timestamp);
}

void
TraceLoggerGraph::stopEvent(uint64_t timestamp)
{
    if (enabled && stack.lastEntry().active()) {
        if (!updateStop(stack.lastEntry().treeId(), timestamp)) {
            fprintf(stderr, "TraceLogging: Failed to stop an event.\n");
            enabled = 0;
            failed = true;
            return;
        }
    }
    if (stack.size() == 1) {
        if (!enabled)
            return;

        // Forcefully disable logging. We have no stack information anymore.
        logTimestamp(TraceLogger_Disable, timestamp);
        return;
    }
    stack.pop();
}

void
TraceLoggerGraph::logTimestamp(uint32_t id, uint64_t timestamp)
{
    if (failed)
        return;

    if (id == TraceLogger_Enable)
        enabled = true;

    if (!enabled)
        return;

    if (id == TraceLogger_Disable)
        disable(timestamp);

    MOZ_ASSERT(eventFile);

    // Format data in big endian
    timestamp = NativeEndian::swapToBigEndian(timestamp);
    id = NativeEndian::swapToBigEndian(id);

    // The layout of the event log in the log file is:
    // [timestamp, textId]
    size_t itemsWritten = 0;
    itemsWritten += fwrite(&timestamp, sizeof(uint64_t), 1, eventFile);
    itemsWritten += fwrite(&id, sizeof(uint32_t), 1, eventFile);
    if (itemsWritten < 2) {
        failed = true;
        enabled = false;
    }
}

bool
TraceLoggerGraph::getTreeEntry(uint32_t treeId, TreeEntry* entry)
{
    // Entry is still in memory
    if (treeId >= treeOffset) {
        *entry = tree[treeId - treeOffset];
        return true;
    }

    int success = fseek(treeFile, treeId * sizeof(TreeEntry), SEEK_SET);
    if (success != 0)
        return false;

    size_t itemsRead = fread((void*)entry, sizeof(TreeEntry), 1, treeFile);
    if (itemsRead < 1)
        return false;

    entryToSystemEndian(entry);
    return true;
}

bool
TraceLoggerGraph::saveTreeEntry(uint32_t treeId, TreeEntry* entry)
{
    int success = fseek(treeFile, treeId * sizeof(TreeEntry), SEEK_SET);
    if (success != 0)
        return false;

    entryToBigEndian(entry);

    size_t itemsWritten = fwrite(entry, sizeof(TreeEntry), 1, treeFile);
    if (itemsWritten < 1)
        return false;

    return true;
}

bool
TraceLoggerGraph::updateHasChildren(uint32_t treeId, bool hasChildren)
{
    if (treeId < treeOffset) {
        TreeEntry entry;
        if (!getTreeEntry(treeId, &entry))
            return false;
        entry.setHasChildren(hasChildren);
        if (!saveTreeEntry(treeId, &entry))
            return false;
        return true;
    }

    tree[treeId - treeOffset].setHasChildren(hasChildren);
    return true;
}

bool
TraceLoggerGraph::updateNextId(uint32_t treeId, uint32_t nextId)
{
    if (treeId < treeOffset) {
        TreeEntry entry;
        if (!getTreeEntry(treeId, &entry))
            return false;
        entry.setNextId(nextId);
        if (!saveTreeEntry(treeId, &entry))
            return false;
        return true;
    }

    tree[treeId - treeOffset].setNextId(nextId);
    return true;
}

bool
TraceLoggerGraph::updateStop(uint32_t treeId, uint64_t timestamp)
{
    if (treeId < treeOffset) {
        TreeEntry entry;
        if (!getTreeEntry(treeId, &entry))
            return false;
        entry.setStop(timestamp);
        if (!saveTreeEntry(treeId, &entry))
            return false;
        return true;
    }

    tree[treeId - treeOffset].setStop(timestamp);
    return true;
}

void
TraceLoggerGraph::disable(uint64_t timestamp)
{
    MOZ_ASSERT(enabled);
    while (stack.size() > 1)
        stopEvent(timestamp);

    enabled = false;
}

void
TraceLoggerGraph::log(ContinuousSpace<EventEntry>& events)
{
    for (uint32_t i = 0; i < events.size(); i++) {
        if (events[i].textId == TraceLogger_Stop)
            stopEvent(events[i].time);
        else if (TLTextIdIsTreeEvent(events[i].textId))
            startEvent(events[i].textId, events[i].time);
        else
            logTimestamp(events[i].textId, events[i].time);
    }
}

void
TraceLoggerGraph::addTextId(uint32_t id, const char* text)
{
    if (failed)
        return;

    // Assume ids are given in order. Which is currently true.
#ifdef DEBUG
    MOZ_ASSERT(id == nextTextId);
    nextTextId++;
#endif

    if (id > 0) {
        int written = fprintf(dictFile, ",\n");
        if (written < 0) {
            failed = true;
            return;
        }
    }

    if (!js::FileEscapedString(dictFile, text, strlen(text), '"'))
        failed = true;
}

#undef getpid
