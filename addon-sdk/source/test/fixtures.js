/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { uri } = module;
const prefix = uri.substr(0, uri.lastIndexOf("/") + 1) + "fixtures/";

exports.url = (path="") => path && path.contains(":")
  ? path
  : prefix + path.replace(/^\.\//, "");

const base64jpeg = "data:image/jpeg;base64,%2F9j%2F4AAQSkZJRgABAQAAAQABAAD%2F" +
                  "2wBDAAMCAgICAgMCAgIDAwMDBAYEBAQEBAgGBgUGCQgKCgkICQkKDA8MCg" +
                  "sOCwkJDRENDg8QEBEQCgwSExIQEw8QEBD%2F2wBDAQMDAwQDBAgEBAgQCw" +
                  "kLEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQ" +
                  "EBAQEBAQEBD%2FwAARCAAgACADAREAAhEBAxEB%2F8QAHwAAAQUBAQEBAQ" +
                  "EAAAAAAAAAAAECAwQFBgcICQoL%2F8QAtRAAAgEDAwIEAwUFBAQAAAF9AQ" +
                  "IDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRol" +
                  "JicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eX" +
                  "qDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJ" +
                  "ytLT1NXW19jZ2uHi4%2BTl5ufo6erx8vP09fb3%2BPn6%2F8QAHwEAAwEB" +
                  "AQEBAQEBAQAAAAAAAAECAwQFBgcICQoL%2F8QAtREAAgECBAQDBAcFBAQA" +
                  "AQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNO" +
                  "El8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0" +
                  "dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6ws" +
                  "PExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3%2BPn6%2F9oADAMB" +
                  "AAIRAxEAPwD5Kr8kP9CwoA5f4m%2F8iRqX%2FbH%2FANHJXr5F%2FwAjCn" +
                  "8%2F%2FSWfnnir%2FwAkji%2F%2B4f8A6dgeD1%2BiH8bn1BX5If6FmFqW" +
                  "pXtveyQwzbUXGBtB7D2r9l4U4UyjMsoo4rFUeacua75pLaUktFJLZH5NxN" +
                  "xNmmX5pVw2Gq8sI8tlyxe8U3q03uzD8S3dxqOi3NneSeZDJs3LgDOHBHI5" +
                  "6gV%2BkcG%2BH%2FDmJzuhSq4e8XzfbqfyS%2FvH5rx1xTm2MyDEUa1W8X" +
                  "yXXLFbTi%2BkThv7B0r%2FAJ9f%2FH2%2Fxr90%2FwCIVcI%2F9An%2FAJ" +
                  "Uq%2FwDyZ%2FO%2F16v%2FADfgv8j0r%2FhZvgj%2FAKDf%2FktN%2FwDE" +
                  "V%2Fnr%2FYWYf8%2B%2Fxj%2Fmf3R%2FxFXhH%2FoL%2FwDKdX%2F5Azrv" +
                  "xLouo3D3lne%2BZDJja3luM4GDwRnqDX9LeH%2FBud4nhzD1aVC8Xz%2Fa" +
                  "h%2Fz8l%2FePx%2FinjrIMZm1WtRxF4vls%2BSa2jFdYlDUdRsp7OSKKbc" +
                  "7YwNpHce1fqfCvCub5bm9HFYqjywjzXfNF7xklopN7s%2BC4l4lyvMMrq4" +
                  "fD1bzfLZcsltJPqktkYlfsZ%2BUnBV%2FnufVnXaD%2FAMgqD%2FgX%2Fo" +
                  "Rr%2BxvCr%2FkkcJ%2F3E%2F8ATsz5%2FHfx5fL8kX6%2FQjkCgD%2F%2F" +
                  "2Q%3D%3D";
exports.base64jpeg = base64jpeg;
