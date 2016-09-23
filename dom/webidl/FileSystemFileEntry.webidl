/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback interface BlobCallback {
    void handleEvent(Blob? blob);
};

interface FileSystemFileEntry : FileSystemEntry {
    // the successCallback should be a FileWriteCallback but this method is not
    // implemented. ErrorCallback will be called with SecurityError.
    void createWriter (VoidCallback successCallback,
                       optional ErrorCallback errorCallback);

    [BinaryName="GetFile"]
    void file (BlobCallback successCallback,
               optional ErrorCallback errorCallback);
};
