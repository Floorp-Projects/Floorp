package com.netscape.jndi.ldap;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.spi.*;
import netscape.ldap.*;
import netscape.ldap.util.*;
import java.io.*;
import java.util.*;
import com.netscape.jndi.ldap.common.Debug;

/**
 * Class used to map Java objects to ldap entries back and forth
*/
public class ObjectMapper {

	/**
	 * Schema object classes for mapping java objects to ldap entries
	 */
	static final String OC_JAVAOBJECT = "javaObject";  // abstract oc
	static final String OC_SERIALOBJ  = "javaSerializedObject";//aux oc
	static final String OC_REFERENCE  = "javaNamingReference"; //aux oc
	static final String OC_REMOTEOBJ  = "javaRemoteObject";    //aux oc
	static final String OC_CONTAINER  = "javaContainer";  //structural oc

	/**
	 * Schema attributes for mapping java objects to ldap entries
	 */
	static final String AT_CLASSNAME   = "javaClassName";	     //required
	static final String AT_CODEBASE    = "javaCodeBase";	     //optional
	static final String AT_SERIALDATA  = "javaSerializedData";   //required
	static final String AT_REFADDR     = "javaReferenceAddress"; //optional
	static final String AT_OBJFACTORY  = "javaFactory";          //optional 
	static final String AT_REMOTELOC   = "javaRemoteLocation";   //required
	
	//Default Object class for NameClassPair
	static final String DEFAULT_OBJCLASS = "javax.naming.directory.DirContext";
	
	static Object entryToObject(LDAPEntry entry, LdapContextImpl ctx) throws NamingException {
		Object obj = entryToObject(entry);
		if (obj == null) {
			obj = new LdapContextImpl(entry.getDN(), ctx);
		}	

		// SP is required to contact the NamingManager first to obtain an object 
		try {
			String relName = LdapNameParser.getRelativeName(ctx.m_ctxDN, entry.getDN());
			Name nameObj = LdapNameParser.getParser().parse(relName);
			obj = NamingManager.getObjectInstance(obj, nameObj, ctx, ctx.getEnvironment());
		}
		catch (Exception ex) {
			if (ex instanceof NamingException) {
				throw (NamingException)ex;
			}
			NamingException nameEx = new NamingException("NamingManager.getObjectInstance failed");
			nameEx.setRootCause(ex);
			throw nameEx;
		}
		
		return obj;

	}

	/**
	 * Convert a Ldap entry into a java object
	 */
	static Object entryToObject(LDAPEntry entry) throws NamingException {
				
		try { 
			LDAPAttributeSet attrs = entry.getAttributeSet();
		
			// TODO javaCodeBase to be processed
			
			LDAPAttribute attr = null;
			if ((attr = attrs.getAttribute(AT_SERIALDATA)) != null) {
				byte[] serdata = (byte[])attr.getByteValues().nextElement();
				return deserializeObject(serdata);
			}
			
			// Reference
			else if ((attr = attrs.getAttribute(AT_REFADDR)) != null) {
				return decodeRefObj(attrs);
			}
						
			// RMI Object						
			else if ((attr = attrs.getAttribute(AT_REMOTELOC)) != null) {
				String rmiURL = (String)attr.getByteValues().nextElement();
				return java.rmi.Naming.lookup(rmiURL);
			}

			return null;
		}
		catch (Exception ex) {
			if (ex instanceof NamingException) {
				throw (NamingException)ex;
			}
			NamingException nameEx = new NamingException("NamingManager.getStateToStore failed");
			nameEx.setRootCause(ex);
			throw nameEx;
		}	
	}

	/**
	 * Get the className for NameClassPair from an entry
	 */
	static String getClassName(LDAPEntry entry) {
		LDAPAttributeSet attrs = entry.getAttributeSet();
		LDAPAttribute attrClass = attrs.getAttribute(AT_CLASSNAME);
		if (attrClass != null) {
			String className = (String)attrClass.getStringValues().nextElement();
			return className;
		}	
		return DEFAULT_OBJCLASS;
	}
	

	/**
	 * Convert a java object with an optional set of attributes into a LDAP entry
	 */
	static LDAPAttributeSet objectToAttrSet(Object obj, String name, LdapContextImpl ctx,  Attributes attrs) throws NamingException {
	
		// SP is required to contact the NamingManager first to obtain a state object 
		try {
			Name nameObj = LdapNameParser.getParser().parse(name);
			obj = NamingManager.getStateToBind(obj, nameObj, ctx, ctx.getEnvironment());
		}
		catch (Exception ex) {
			if (ex instanceof NamingException) {
				throw (NamingException)ex;
			}
			NamingException nameEx = new NamingException("NamingManager.getStateToStore failed");
			nameEx.setRootCause(ex);
			throw nameEx;
		}

		if (attrs == null) {
			attrs = new BasicAttributes(/*ignoreCase=*/true);
		}	

		Attribute objectclass = attrs.get("objectClass");
		if (objectclass == null) {
			objectclass = new BasicAttribute("objectClass", "top");
			attrs.put(objectclass);
		}
		
		// Root object class
		objectclass.add(OC_JAVAOBJECT);

		if (obj instanceof Reference) {
			objectclass.add(OC_REFERENCE);
			char delimChar = ctx.m_ctxEnv.getRefSeparator();
			attrs = encodeRefObj(delimChar, (Reference)obj, attrs);
		}
		
		else if (obj instanceof Referenceable) {
			objectclass.add(OC_REFERENCE);
			char delimChar = ctx.m_ctxEnv.getRefSeparator();
			attrs = encodeRefObj(delimChar, ((Referenceable)obj).getReference(), attrs);
		}
		
		else  if (obj instanceof java.rmi.Remote) {
			objectclass.add(OC_REMOTEOBJ);
			attrs = encodeRMIObj((java.rmi.Remote)obj, attrs);
		}
		
		else if (obj instanceof Serializable) {
			objectclass.add(OC_SERIALOBJ);
			attrs = encodeSerialObj((Serializable)obj , attrs);
		}
		
		else if (obj instanceof DirContext) {
			attrs = encodeDirCtxObj((DirContext)obj , attrs);
		}
		
		else {
			throw new NamingException("Can not bind object of type " +
			      obj.getClass().getName());
		}	
			
		return AttributesImpl.jndiAttrsToLdapAttrSet(attrs);
	}	

	/**
	 * Deserialized object, create object from a stream of bytes
	 */
	private static Object deserializeObject(byte[] byteBuf) throws NamingException {
	
		ByteArrayInputStream bis = null;
		ObjectInputStream  objis = null;

		try {
			bis   = new ByteArrayInputStream(byteBuf);
			objis = new ObjectInputStream(bis);
            return objis.readObject();
		}
		catch(Exception ex) {
			NamingException nameEx = new NamingException("Failed to deserialize object");
			nameEx.setRootCause(ex);
			throw nameEx;
		}
		finally {
			try {
				if (objis != null) {
					objis.close();
				}
				if (bis != null) {
					bis.close();
				}
			}
			catch (Exception ex) {}
        }
	}


	/**
	 * Serialize object, convert object to a stream of bytes
	 */
	private static byte[] serializeObject(Object obj) throws NamingException {

		ByteArrayOutputStream bos = null;
		ObjectOutputStream  objos = null;

		try {
			bos   = new ByteArrayOutputStream();
			objos = new ObjectOutputStream(bos);
			objos.writeObject(obj);
			objos.flush();
            return bos.toByteArray();
        }
		catch(Exception ex) {
			NamingException nameEx = new NamingException("Failed to serialize object");
			nameEx.setRootCause(ex);
			throw nameEx;
		}
		finally {
			try {
				if (objos != null) {
					objos.close();
				}
				if (bos != null) {
					bos.close();
				}
			}
			catch (Exception ex) {}
        }
    }	

	/**
	 * Decode Jndi Reference Object
	 */
	private static Reference decodeRefObj(LDAPAttributeSet attrs) throws NamingException {		
		try {

        	LDAPAttribute attr = null;
        	String className=null, factory = null, factoryLoc = null; 
			if ((attr = attrs.getAttribute(AT_CLASSNAME)) == null ) {
				throw new NamingException("Bad Reference encoding, no attribute " + AT_CLASSNAME);
			}	
			className = (String)attr.getStringValues().nextElement();
			
			if ((attr = attrs.getAttribute(AT_OBJFACTORY)) != null ) {
				factory = (String)attr.getStringValues().nextElement();
			}	
			if ((attr = attrs.getAttribute(AT_CODEBASE)) != null ) {
				factoryLoc = (String)attr.getStringValues().nextElement();
			}	

			Reference ref = new Reference(className, factory, factoryLoc);
			
			if ((attr = attrs.getAttribute(AT_REFADDR)) == null ) {
				return ref; // no refAddr
			}	

			for (Enumeration e = attr.getStringValues(); e.hasMoreElements();) {
				decodeRefAddr((String)e.nextElement(), ref);
			}
			
			return ref;
		}	
		catch (Exception ex) {
			if (ex instanceof NamingException) {
				throw (NamingException)ex;
			}
			NamingException nameEx = new NamingException("NamingManager.getStateToStore failed");
			nameEx.setRootCause(ex);
			throw nameEx;
		}	
	}

	/**
	 * Decode RefAddr according to the <draft-ryan-java-schema-01.rev.txt>
	 * StringRefAddr are encoded as #posNo#refType#refValue where posNo is the 
	 * refAddr position (index) inside a rerference. BynaryRefAddr is encoded
	 * as #posNo#refType##data where data is the base64 encoding of the serialized
	 * form of the entire BinaryRefAddr instance.
	 */	
    private static void decodeRefAddr(String enc, Reference ref) throws NamingException{
    	
    	if (enc.length() == 0) {
    		throw new NamingException("Bad Reference encoding, empty refAddr");
    	}
    	
    	String delimChar = enc.substring(0,1);
    	
		StringTokenizer tok = new StringTokenizer(enc, delimChar);
		
		int tokCount = tok.countTokens();
		if (tokCount != 3 && tokCount != 4)  {
			Debug.println(3, "enc=" + enc + " delimChar="+delimChar + " tokCount="+tokCount);
			throw new NamingException("Bad Reference encoding");
		}
		
		String type = null;
		int posn = -1;
		for (int i = 0; i < tokCount; i++)	{
			String s = tok.nextToken();
		
			if (i == 0) { // position
				try {
					posn = Integer.parseInt(s);
				}
				catch (Exception e) {
					throw new NamingException("Bad Reference encoding, refAddr position not an initger");
				}
			}
			
			else if (i == 1) { // type
				if (s.length() == 0) {
					throw new NamingException("Bad Reference encoding, empty refAddr type");
				}
				type = s;
			}
			
			else if (i == 2) { // value for StringRefAddr, empty for BinaryRefAddr
				if (s.length() == 0 && tokCount != 4) { // should be empty for binary refs
					throw new NamingException("Bad Reference encoding, empty refAddr string value");
				}
				ref.add(posn, new StringRefAddr(type, s));
			}
			
			else { // base64 encoding of the serialized BinaryRefAddr
				if (s.length() == 0) {
					throw new NamingException("Bad Reference encoding, empty refAddr binary value");
				}
				MimeBase64Decoder base64Dec = new MimeBase64Decoder();
				ByteBuf in  = new ByteBuf(s), out = new ByteBuf();
				base64Dec.translate(in, out);
				base64Dec.eof(out);
				ref.add(posn, (RefAddr) deserializeObject(out.toBytes()));
			}
		}
	}	


	/**
	 * Encode Jndi Reference Object
	 */
	private static Attributes encodeRefObj(char delimChar, Reference ref, Attributes attrs) throws NamingException {
        
		if (ref.getClassName() != null) {
			attrs.put(new BasicAttribute(AT_CLASSNAME, ref.getClassName()));
		}
        if (ref.getFactoryClassName() != null) {
			attrs.put(new BasicAttribute(AT_OBJFACTORY, ref.getFactoryClassName()));
		}	
        if (ref.getFactoryClassLocation() != null) {
			attrs.put(new BasicAttribute(AT_CODEBASE, ref.getFactoryClassLocation()));
		}	

		if(ref.size() > 0) {
            BasicAttribute refAttr = new BasicAttribute(AT_REFADDR);
            for(int i = 0; i < ref.size(); i++) {
				refAttr.add(encodeRefAddr(delimChar, i, ref.get(i)));
			}
			attrs.put(refAttr);
		}
		return attrs;
	}

	/**
	 * Encode RefAddr according to the <draft-ryan-java-schema-01.rev.txt>
	 * StringRefAddr are encoded as #posNo#refType#refValue where posNo is the 
	 * refAddr position (index) inside a rerference. BynaryRefAddr is encoded
	 * as #posNo#refType##data where data is the base64 encoding of the serialized
	 * form of the entire BinaryRefAddr instance.

	 */	
    private static String encodeRefAddr(char delimChar, int idx, RefAddr refAddr) throws NamingException{
    	
		if(refAddr instanceof StringRefAddr) {

			String content = (String) refAddr.getContent();
			if (content != null && content.length() > 0 && content.charAt(0) == delimChar) {
				throw new NamingException(
				"Can not encode StringRefAddr, value starts with the delimiter character " + delimChar);
			}	
        	return delimChar + idx +
        	       delimChar + refAddr.getType() +
        	       delimChar + content;
		}
		
		else { // BinaryRefAdd
		
			byte[] serialRefAddr = serializeObject(refAddr);
			MimeBase64Encoder base64 = new MimeBase64Encoder();
			ByteBuf in  = new ByteBuf(), out = new ByteBuf();
			in.append(serialRefAddr);
			base64.translate(in, out);
			base64.eof(out);
        	return delimChar + idx +
        	       delimChar + refAddr.getType() +
        	       delimChar + delimChar + out.toString();
        }
    }
    
    
    /**
     * Encode Serializable object
     */
     
     static Attributes encodeSerialObj(Serializable obj, Attributes attrs) throws NamingException{
		if (attrs.get(AT_CLASSNAME) == null) {
			attrs.put(new BasicAttribute(AT_CLASSNAME, obj.getClass().getName()));
		}
		attrs.put(new BasicAttribute(AT_SERIALDATA, serializeObject(obj)));
		return attrs;
	 }
	 
    /**
     * Encode RMI Object
     */
	static Attributes encodeRMIObj(java.rmi.Remote obj, Attributes attrs) throws NamingException{
		if (attrs.get(AT_REMOTELOC) == null) {
			throw new NamingException(
			"Can not bind Remote object, " 	+ AT_REMOTELOC + " not specified");
		}
		if (attrs.get(AT_CLASSNAME) == null) {
			attrs.put(new BasicAttribute(AT_CLASSNAME, obj.getClass().getName()));
		}
		return attrs;
	}	
     
    /**
     * Encode DirContext object (merege attributes)
     */
	static Attributes encodeDirCtxObj(DirContext obj, Attributes attrs) throws NamingException{
		Attributes ctxAttrs = obj.getAttributes("");
		for (NamingEnumeration enum = ctxAttrs.getAll(); enum.hasMore();) {
			attrs.put((Attribute)enum.next());
		}
		return attrs;
	}
    
    public static void main(String[] args) {
			byte[] serialRefAddr = { (byte)'a', (byte)'0', (byte)'A', (byte)0x10, (byte)0x7f, (byte)0xaa };
			MimeBase64Encoder base64 = new MimeBase64Encoder();
			MimeBase64Decoder base64Dec = new MimeBase64Decoder();
			ByteBuf in  = new ByteBuf(), out = new ByteBuf();
			in.append(serialRefAddr);
			base64.translate(in, out);
			base64.eof(out);
			System.err.println("in="+in);
			System.err.println("out="+out);
			in  = new ByteBuf(out.toString());
			out = new ByteBuf();
			base64Dec.translate(in, out);
			base64Dec.eof(out);
			System.err.println("in="+in);
			System.err.println("out="+out);
			
	}
}

