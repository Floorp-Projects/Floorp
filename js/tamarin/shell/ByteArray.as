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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

package flash.utils
{

//
// ByteArray
//


/**
 * The ByteArray class makes it possible to work with
 * binary data in ActionScript in an efficient manner.
 *
 * <p><b><i>Note:</i></b> The ByteArray class are for advanced ActionScript developers who need to access 
 * data on the byte level.</p>
 *
 * <p>Its in-memory representation is a packed array of bytes, but an instance of the ByteArray
 * class can be manipulated with the standard the ActionScript array access operators: 
 * <code>[</code> and <code>]</code>.</p>
 *
 * <p>It also can be read/written to as an in-memory file, using
 * methods similar to <code>URLStream</code> and <code>Socket</code>.</p>
 * 
 * <p>In addition, zlib compression/decompression is supported, as
 * well as AMF object serialization.</p>
 *
 * <p>Possible uses of the <code>ByteArray</code> class include the following:
 *
 * <ul>
 *
 *	 <li>Creating a custom protocol to connect to a server.</li>
 *
 *	 <li>Writing your own URLEncoder/URLDecoder.</li>
 *
 *	 <li>Writing your own AMF/Remoting packet.</li>
 *
 *	 <li>Optimizing the size of your data by using data types.</li>
 *
 * </ul>
 * </p>
 *
 * @playerversion Flash 8.0
 * @langversion 3.0
 * @helpid
 * @refpath 
 * @keyword ByteArray
 */
public class ByteArray
{
	public native static function readFile(filename:String):ByteArray;
	public native function writeFile(filename:String):void;

	/**
	 * Reads <code>length</code> bytes of data from the byte stream.
	 * The bytes are read into the <code>ByteArray</code> object specified
	 * by <code>bytes</code>, starting <code>offset</code> bytes into
	 * the <code>ByteAray</code>.
	 *
	 * @param bytes The <code>ByteArray</code> object to read
	 *              data into.
	 * @param offset The offset into <code>bytes</code> at which data
	 *               read should begin.  Defaults to 0.
	 * @param length The number of bytes to read.  The default value
	 *               of 0 will cause all available data to be read.
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 * @throws IOError An I/O error occurred on the byte stream,
	 * or the byte stream is not open.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readBytes, readBytes
	 */	
	public native function readBytes(bytes:ByteArray,
									 offset:uint=0,
									 length:uint=0):void;


	/**
	 * Writes a sequence of <code>length</code> bytes from the
	 * specified byte array, <code>bytes</code>,
	 * starting <code>offset</code>(zero-based index) bytes
	 * into the array.
	 *
	 * <p><code>offset</code> and <code>length</code> are optional
	 * parameters. If <code>length</code> is omitted, the default
	 * length of 0 means to write the entire buffer starting at
	 * <code>offset</code>.
	 *
	 * If <code>offset</code> is also omitted, the entire buffer is
	 * written. </p> <p>If <code>offset</code> or <code>length</code>
	 * is out of range, they will be clamped to the beginning and end
	 * of the <code>bytes</code> array.</p>
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeBytes, writeBytes
	 */	
	public native function writeBytes(bytes:ByteArray,
									  offset:uint=0,
									  length:uint=0):void;

	/**
	 * Writes a boolean to the byte stream. A single byte is written,
	 * either 1 for <code>true</code> or 0 for <code>false</code>.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeBoolean, writeBoolean
	 */	
	public native function writeBoolean(value:Boolean):void;
	
	/**
	 * Writes a byte to the byte stream. 
	 * <p>The low 8 bits of the
	 * parameter are used. The high 24 bits are ignored. </p>
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeByte, writeByte
	 */	
	public native function writeByte(value:int):void;

	/**
	 * Writes a 16-bit integer to the byte stream. The bytes written
	 * are:
	 *
	 * <pre><code>(v &gt;&gt; 8) &amp; 0xff v &amp; 0xff</code></pre>
	 *
	 * <p>The low 16 bits of the parameter are used. The high 16 bits
	 * are ignored.</p>
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeShort, writeShort
	 */	
	public native function writeShort(value:int):void;

	/**
	 * Writes a 32-bit signed integer to the byte stream.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeInt, writeInt
	 */	
	public native function writeInt(value:int):void;
	
	/**
	 * Writes a 32-bit unsigned integer to the byte stream.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeUnsignedInt, writeUnsignedInt
	 */	
	public native function writeUnsignedInt(value:uint):void;

	/**
	 * Writes an IEEE 754 single-precision floating point number to the byte stream. 
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeFloat, writeFloat
	 */	
	public native function writeFloat(value:Number):void;

	/**
	 * Writes an IEEE 754 double-precision floating point number to the byte stream. 
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeDouble, writeDouble
	 */	
	public native function writeDouble(value:Number):void;

	/**
	 * Writes a 16-bit unsigned integer to the byte stream, specifying
	 * the length of the UTF-8 string that follows in bytes. Then,
	 * writes the UTF-8 string to the byte stream.
	 *
	 * <p>First, the total number of bytes needed to represent all the
	 * characters of <code>s</code> is calculated.</p>
	 *
	 * @throws RangeError If the length is larger than
	 * <code>65535</code>.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeUTF, writeUTF
	 */	
	public native function writeUTF(value:String):void;

	/**
	 * Writes a UTF-8 string to the byte stream. Similar to writeUTF,
	 * but does not prefix the string with a 16-bit length word.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.writeUTFBytes, writeUTFBytes
	 */	
	public native function writeUTFBytes(value:String):void;
	
	/**
	 * Reads a boolean from the byte stream. A single byte is read,
	 * and <code>true</code> is returned if the byte is non-zero,
	 * <code>false</code> otherwise.
	 * 
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readBoolean, readBoolean
	 */	
	public native function readBoolean():Boolean;

	/**
	 * Reads a signed byte from the byte stream.
	 * <p>The returned value is in the range -128...127.</p>
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readByte, readByte
	 */	
	public native function readByte():int;

	/**
	 * Reads an unsigned byte from the byte stream.
	 * 
	 * <p>The returned value is in the range 0...255. </p>	
	 *
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readUnsignedByte, readUnsignedByte
	 */	
	public native function readUnsignedByte():uint;

	/**
	 * Reads a signed 16-bit integer from the byte stream.
	 *
	 * <p>The returned value is in the range -32768...32767.</p>
	 *
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readShort, readShort
	 */	
	public native function readShort():int;


	/**
	 * Reads an unsigned 16-bit integer from the byte stream.
	 *
	 * <p>The returned value is in the range 0...65535. </p>
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readUnsignedShort, readUnsignedShort
	 */	
	public native function readUnsignedShort():uint;
	
	/**
	 * Reads a signed 32-bit integer from the byte stream.
	 *
     * <p>The returned value is in the range -2147483648...2147483647.</p>
	 *
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readInt, readInt
	 */	
	public native function readInt():int;

	/**
	 * Reads an unsigned 32-bit integer from the byte stream.
	 *
	 * <p>The returned value is in the range 0...4294967295. </p>
	 *
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readUnsignedInt, readUnsignedInt
	 */	
	public native function readUnsignedInt():uint;

	/**
	 * Reads an IEEE 754 single-precision floating point number from the byte stream.
	 *
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readFloat, readFloat
	 */	
	public native function readFloat():Number;

	/**
	 * Reads an IEEE 754 double-precision floating point number from the byte stream.
	 *
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readDouble, readDouble
	 */	
	public native function readDouble():Number;

	/**
	 * Reads a UTF-8 string from the byte stream.  The string
	 * is assumed to be prefixed with an unsigned short indicating
	 * the length in bytes.
	 *
	 * <p>This method is similar to the <code>readUTF</code>
	 * method in the Java <code>DataInput</code> interface.</p>
	 *
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readUTF, readUTF
	 */	
	public native function readUTF():String;

	/**
	 * Reads a sequence of <code>length</code> UTF-8
	 * bytes from the byte stream, and returns a string.
	 *
	 * @throws EOFError There is not sufficient data available
	 * to read.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.readUTFBytes, readUTFBytes
	 */	
	public native function readUTFBytes(length:uint):String;

	/**
	 * The length of the <code>ByteArray</code>, in bytes.
	 *
	 * <p>If the length is set to a larger value than the
	 * current length, the empty space is filled with zeros.</p>
	 *
	 * <p>If the length is set to a smaller value than the
	 * current length, the array is truncated.</p>
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.length, length
	 */	
	public native function get length():uint;
	public native function set length(value:uint):void;

	/**
	* Compresses the byte array using zlib compression.
	* The entire byte array is compressed.
	*
	* @see #uncompress
	*
	* @playerversion Flash 8.0
	* @langversion 3.0
	* @helpid
	* @refpath
	* @keyword ByteArray, ByteArray.compress, compress
	*/
	public native function compress():void;

	/**
	* Uncompresses the byte array.  The byte array
	* must have been previously compressed with the
	* <code>compress</code> method.
	*
	* @see #compress
	*
	* @playerversion Flash 8.0
	* @langversion 3.0
	* @helpid
	* @refpath
	* @keyword ByteArray, ByteArray.uncompress, uncompress
	*/
	public native function uncompress():void;


	/**
	 * Converts the ByteArray to a String.
	 *
	 * @return The String representation of the ByteArray.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.toString, toString
	 */	
	public native function toString():String;
	
	/**
	 * The number of bytes of data available for reading
	 * from the current position in the byte array to the
	 * end of the array.
	 *
	 * <p>User code must access the <code>bytesAvailable</code> property to ensure
	 * that sufficient data is available before trying to read
	 * it with one of the <code>read</code> methods.</p>
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword available, bytes, position 
	 */	
	public native function get bytesAvailable():uint;

	/**
	 * Returns the current position, in bytes, of the file
	 * pointer into the <code>ByteArray</code>.  This is the
	 * point at which the next call to a <code>read</code>
	 * method will start reading, or a <code>write</code>
	 * method will start writing.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.getFilePointer, 
	 */	
	public native function get position():uint;

	/**
	 * Moves the file pointer to <code>offset</code> bytes
	 * into the byte array.  The next call to a <code>read</code>
	 * or <code>write</code> method will start operating
	 * at this point.
	 *
	 * @playerversion Flash 8.0
	 * @langversion 3.0
	 * @helpid
	 * @refpath 
	 * @keyword ByteArray, ByteArray.seek, 
	 */	
	public native function set position(offset:uint):void;

	public native function get endian():String;
	public native function set endian(type:String):void;
};



/*
 * [ggrossman 04/07/05] API SCRUB
 *
 * - _ByteArray_ now implements the _DataInput_ and _DataOutput_
 *   interfaces, as described in the Low Level Data specification.
 *
 * - Method _available()_ changed to accessor _bytesAvailable_
 * - Method _getFilePointer()_ changed to accessor _position_
 * - Method _seek()_ changed to accessor _position_
 * - Method _readUnsignedByte()_ now returns type _uint_
 * - Method _readUnsignedShort()_ now returns type _uint_
 *
 * - Renamed _flash_transient_ namespace to _transient_ and
 *   put in _flash.net_ package
 *
 * - Moved _registerClass_ to the _flash.net_ package.
 *
 * - Moved the _ObjectEncoding_ class from _flash.util_ to the
 *   _flash.net_ package.
 *
 * - [srahim 04/05/05] Doc scrub
 */

}
