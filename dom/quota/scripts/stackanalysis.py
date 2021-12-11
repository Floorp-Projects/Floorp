# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# There seem to be sometimes identical events recorded twice by telemetry
def sanitize(rows):
    newrows = []
    pcid = "unset"
    psid = "unset"
    pseq = "unset"
    for row in rows:
        cid = row["client_id"]
        sid = row["session_id"]
        seq = row["seq"]
        if cid != pcid or sid != psid or seq != pseq:
            newrows.append(row)
        pcid = cid
        psid = sid
        pseq = seq

    return newrows


# Given a set of rows, find all distinct build ids
def extractBuildIDs(rows):
    buildids = {}
    for row in rows:
        id = row["build_id"]
        if id in buildids:
            buildids[id] = buildids[id] + 1
        else:
            buildids[id] = 1
    return buildids


# Given a set of build ids and rows, enrich each row by an hg link.
# Relys on the result of utils.fetchBuildRevisions in buildids.
def constructHGLinks(buildids, rows):
    for row in rows:
        id = row["build_id"]
        if id in buildids:
            row["location"] = (
                buildids[id] + "/" + row["source_file"] + "#l" + row["source_line"]
            )
        else:
            row["location"] = id + "/" + row["source_file"] + "#l" + row["source_line"]


topmost_stackframes = set()
delta_frames = {}


def isTopmostFrame(frame):
    f = (frame["location"], frame["result"])
    return f in topmost_stackframes


def addTopmostFrame(frame):
    f = (frame["location"], frame["result"])
    if not isTopmostFrame(frame):
        # print("Found new topmost frame {}.".format(frame))
        topmost_stackframes.add(f)
        frame["topmost"] = True


def addFrameDelta(frame1, frame2):
    if frame1["client_id"] != frame2["client_id"]:
        return
    if frame1["session_id"] != frame2["session_id"]:
        return

    fkey = "{}:{}-{}:{}".format(
        frame2["location"], frame2["result"], frame1["location"], frame1["result"]
    )
    if fkey not in delta_frames:
        fdelta = {"delta_sum": 0, "delta_cnt": 0}
        fdelta["prev_row"] = frame1
        fdelta["candidate"] = frame2
        delta_frames[fkey] = fdelta

    fdelta = delta_frames[fkey]
    etv1 = frame1["event_timestamp"]
    etv2 = frame2["event_timestamp"]
    if isinstance(etv1, int) and isinstance(etv2, int) and etv2 > etv1:
        delta = etv2 - etv1
        fdelta["delta_sum"] = fdelta["delta_sum"] + delta
        fdelta["delta_cnt"] = fdelta["delta_cnt"] + 1


# There can be outliers in terms of time distance between two stack frames
# that belong to the same propagation stack. In order to not increase the
# risk that one outlier breaks thousands of stacks, we check for the average
# time distance.
def checkAverageFrameTimeDeltas(rows, max_delta):
    # print("checkAverageFrameTimeDeltas")
    prev_row = None
    for row in rows:
        if "topmost" in row or not row["session_complete"]:
            prev_row = None
            continue

        if prev_row:
            addFrameDelta(prev_row, row)
        prev_row = row

    for fd in delta_frames:
        sum = delta_frames[fd]["delta_sum"]
        cnt = delta_frames[fd]["delta_cnt"]
        if cnt > 0 and (sum / cnt) > max_delta:
            # print(delta_frames[fd])
            addTopmostFrame(delta_frames[fd]["candidate"])


# A topmost frame is considered to initiate a new raw stack. We collect all
# candidates before we actually apply them. This implies, that we should run
# this function on a "large enough" sample of rows to be more accurate.
# As a side effect, we mark all rows that are part of a "complete" session
# (a session, that started within our data scope).
def collectTopmostFrames(rows):
    prev_cid = "unset"
    prev_sid = "unset"
    prev_ctx = "unset"
    prev_sev = "ERROR"
    session_complete = False
    after_severity_downgrade = False
    for row in rows:
        cid = row["client_id"]
        sid = row["session_id"]
        ctx = row["context"]
        seq = row["seq"]
        sev = row["severity"]

        # If we have a new session, ensure it is complete from start,
        # otherwise we will ignore it entirely.
        if cid != prev_cid or sid != prev_sid:
            if seq == 1:
                session_complete = True
            else:
                session_complete = False
        row["session_complete"] = session_complete
        if session_complete:
            # If we change client, session or context, we can be sure to have
            # a new topmost frame.
            if seq == 1 or cid != prev_cid or sid != prev_sid or ctx != prev_ctx:
                addTopmostFrame(row)
                after_severity_downgrade = False
            # We do not expect a non-error to be ever upgraded to an error
            elif sev == "ERROR" and prev_sev != "ERROR":
                addTopmostFrame(row)
                after_severity_downgrade = False
            # If we just had a severity downgrade, we assume that we wanted
            # to break the error propagation after this point and split, too
            elif after_severity_downgrade:
                addTopmostFrame(row)
                after_severity_downgrade = False
            elif prev_sev == "ERROR" and sev != "ERROR":
                after_severity_downgrade = True

        prev_cid = cid
        prev_sid = sid
        prev_ctx = ctx
        prev_sev = sev

    # Should be ms. We've seen quite some runtime between stackframes in the
    # wild. We might want to consider to make this configurable. In general
    # we prefer local context over letting slip through some topmost frame
    # unrecognized, assuming that fixing the issues one by one they will
    # uncover them succesively. This is achieved by a rather high delta value.
    max_avg_delta = 200
    checkAverageFrameTimeDeltas(rows, max_avg_delta)


def getFrameKey(frame):
    return "{}.{}|".format(frame["location"], frame["result"])


def getStackKey(stack):
    stack_key = ""
    for frame in stack["frames"]:
        stack_key += getFrameKey(frame)
    return hash(stack_key)


# A "raw stack" is a list of frames, that:
# - share the same build_id (implicitely through location)
# - share the same client_id
# - share the same session_id
# - has a growing sequence number
# - stops at the first downgrade of severity from ERROR to else
# - XXX: contains each location at most once (no recursion)
# - appears to be in a reasonable short timeframe
# Calculates also a hash key to identify identical stacks
def collectRawStacks(rows):
    collectTopmostFrames(rows)
    raw_stacks = []
    stack = {
        "stack_id": "unset",
        "client_id": "unset",
        "session_id": "unset",
        "submit_timeabs": "unset",
        "frames": [{"location": "unset"}],
    }
    stack_id = 1
    first = True
    for row in rows:
        if isTopmostFrame(row):
            if not first:
                stack["stack_key"] = getStackKey(stack)
                raw_stacks.append(stack)
            stack_id += 1
            stack = {
                "stack_id": stack_id,
                "client_id": row["client_id"],
                "session_id": row["session_id"],
                "submit_timeabs": row["submit_timeabs"],
                "context": row["context"],
                "frames": [],
            }

        stack["frames"].append(
            {
                "location": row["location"],
                "source_file": row["source_file"],
                "source_line": row["source_line"],
                "seq": row["seq"],
                "severity": row["severity"],
                "result": row["result"],
            }
        )
        first = False

    return raw_stacks


# Merge all stacks that have the same hash key and count occurences.
# Relys on the ordering per client_id/session_id for correct counting.
def mergeEqualStacks(raw_stacks):
    merged_stacks = {}
    last_client_id = "none"
    last_session_id = "none"
    for stack in raw_stacks:
        stack_key = stack["stack_key"]
        merged_stack = stack
        if stack_key in merged_stacks:
            merged_stack = merged_stacks[stack_key]
            if stack["client_id"] != last_client_id:
                last_client_id = stack["client_id"]
                merged_stack["client_count"] += 1
            if stack["session_id"] != last_session_id:
                last_session_id = stack["session_id"]
                merged_stack["session_count"] += 1
            merged_stack["hit_count"] += 1
        else:
            merged_stack["client_count"] = 1
            last_client_id = merged_stack["client_id"]
            merged_stack["session_count"] = 1
            last_session_id = merged_stack["session_id"]
            merged_stack["hit_count"] = 1
            merged_stacks[stack_key] = merged_stack

    merged_list = list(merged_stacks.values())
    merged_list.sort(key=lambda x: x.get("hit_count"), reverse=True)
    return merged_list


# Split the list of stacks into:
# - aborted (has at least one frame with NS_ERROR_ABORT)
# - info/warning (has at least one frame with that severity)
# - error (has only error frames)
def filterStacksForPropagation(
    all_stacks, error_stacks, warn_stacks, info_stacks, abort_stacks
):
    for stack in all_stacks:
        warn = list(filter(lambda x: x["severity"] == "WARNING", stack["frames"]))
        info = list(filter(lambda x: x["severity"] == "INFO", stack["frames"]))
        abort = list(filter(lambda x: x["result"] == "NS_ERROR_ABORT", stack["frames"]))
        if len(abort) > 0:
            abort_stacks.append(stack)
        elif len(info) > 0:
            info_stacks.append(stack)
        elif len(warn) > 0:
            warn_stacks.append(stack)
        else:
            error_stacks.append(stack)


# Bugzilla comment markup
def printStacks(stacks):
    out = ""
    row_format = "{} | {} | {} | {} | {}\n"
    out += row_format.format("Clients", "Sessions", "Hits", "Anchor", "Stack")
    out += row_format.format("-------", "-------", "--------", "--------", "--------")
    for stack in stacks:
        framestr = ""
        first = True
        for frame in stack["frames"]:
            if not first:
                framestr += " <- "
            framestr += "[{}#{}:{}]({})".format(
                frame["source_file"],
                frame["source_line"],
                frame["result"],
                frame["location"],
            )
            first = False
        out += row_format.format(
            stack["client_count"],
            stack["session_count"],
            stack["hit_count"],
            stack["frames"][0]["anchor"],
            framestr,
        )

    return out


def groupStacksForAnchors(stacks):
    anchors = {}
    for stack in stacks:
        anchor_name = stack["frames"][0]["anchor"]
        if anchor_name in anchors:
            anchors[anchor_name]["stacks"].append(stack)
        else:
            anchor = {"anchor": anchor_name, "stacks": [stack]}
            anchors[anchor_name] = anchor
    return anchors


"""
def getSummaryForAnchor(anchor):
    return "[QM_TRY] Errors in function {}".format(anchor)


def searchBugForAnchor(bugzilla_key, anchor):
    summary = getSummaryForAnchor(anchor)
    bug_url = "https://bugzilla.mozilla.org/rest/bug?" \
              "summary={}&api_key={}".format(summary, bugzilla_key)
    return requests.get(url=bug_url).json()["bugs"]


def createBugForAnchor(bugzilla_key, anchor):
    summary = getSummaryForAnchor(anchor)
    bug_url = "https://bugzilla.mozilla.org/rest/bug?" \
              "Bugzilla_api_key={}".format(bugzilla_key)
    body = {
        "product" : "Core",
        "component" : "Storage: Quota Manager",
        "version" : "unspecified",
        "summary" : summary,
        "description" : "This bug collects errors reported by QM_TRY"
                        "macros for function {}.".format(anchor),
    }
    resp = requests.post(url=bug_url, json=body)
    if resp.status_code != 200:
        print(resp)
        return 0
    id = resp.json()["id"]
    print("Added new bug {}:".format(id))
    return id


def ensureBugForAnchor(bugzilla_key, anchor):
    buglist = searchBugForAnchor(bugzilla_key, anchor)
    if (len(buglist) > 0):
        id = buglist[0]["id"]
        print("Found existing bug {}:".format(id))
        return id
    return createBugForAnchor(bugzilla_key, anchor)


def addCommentForAnchor(bugzilla_key, anchor, stacks):
    id = ensureBugForAnchor(bugzilla_key, anchor)
    if (id <= 0):
        print("Unable to create a bug for {}.".format(anchor))
        return
    comment = printStacks(stacks)
    print("")
    print("Add comment to bug {}:".format(id))
    print(comment)


def addCommentsForStacks(bugzilla_key, stacks):
    anchors = groupStacksForAnchors(stacks)
    for anchor in anchors:
        addCommentForAnchor(bugzilla_key, anchors[anchor]["anchor"], anchors[anchor]["stacks"])
"""
