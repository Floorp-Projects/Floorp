/* ****************************************************************************
 *  
 *  Copyright (c) 1995-1996 by CompuServe Incorporated, Columbus, Ohio
 *  All Rights Reserved
 *
 *  Facility:   CompuServe Distributed Authentication(DA) Facility
 *
 *  Abstract
 *      This facility provides a means for authentication of users and services 
 *      without compromising passphrases between them across the wire.
 *
 *  Module
 *      This module defines the interfaces necessary for the DA process.
 *
 *  Author
 *      Chris Fortier
 *
 *  Notes
 *      DA is implemented as an in-process OLE server.  To access the DA 
 *      OLE server, the IDAProcess interface on an instance of the CLSID_DA 
 *      object must be used.  Using IDAProcess requires the client to implement 
 *      the IDAPacket and optionally the IDAPassword interfaces.  These 
 *      interfaces are described below.  
 *
 *      The DA OLE server is self-registering, so be sure to (un)register the 
 *      DA OLE server by using REGSVR32.EXE, or by accessing the exported 
 *      (DllUnregisterServer)DllRegisterServer functions.
 *
 */


#if !defined(__DAINTF_H__)
#define __DAINTF_H__


/* ****************************************************************************
 *  
 *  Name
 *      IDAPacket
 *
 *  Purpose
 *      This interface must be implemented by clients who wish to access DA 
 *      services.  The interface is used to encapsulate a request or response 
 *      during the DA process.
 *
 *  Notes
 *      The implemented form of a packet could be an HTTP header.  As the 
 *      header representation varies (buffer, tree, etc.), an encapsulation is 
 *      necessary.  Terminology is as follows:
 *
 *          Term                    Example
 *
 *          Name                    Authorization:
 *          Value                   Remote-Passphrase
 *          SubName="SubValue"      Realm="foo"
 *
 */
#undef  INTERFACE
#define INTERFACE   IDAPacket

DECLARE_INTERFACE_(IDAPacket, IUnknown)
{
    /* ************************************************************************
     *
     *  HRESULT IDAPacket::AddNameValue
     *  (
     *      [IN]    const TCHAR *pszName,
     *      [IN]    const TCHAR *pszValue
     *  )
     *
     *  This method adds a new name and value to the DA packet.
     *
     *  Parameters
     *      pszName     - Name to add.
     *      pszValue    - Value to add.
     *
     *  Return
     *      NOERROR     - The name and value were added.
     *      E_FAIL      - The name and value could not be added.
     *
     */
    STDMETHOD(AddNameValue)(THIS_
                            const TCHAR *pszName, 
                            const TCHAR *pszValue)          PURE;

    /* ************************************************************************
     *
     *  HRESULT IDAPacket::FindValue
     *  (
     *      [IN]            const TCHAR *pszName,
     *      [IN]            const TCHAR *pszSubName,
     *      [OUT OPTIONAL]  const TCHAR *pszSubValue,
     *      [IN]            DWORD        cbSubValue
     *  )
     *
     *  This method finds a value or subvalue given a packet header name and 
     *  subname.
     *
     *  Parameters
     *      pszName     - Name of the header within which to search
     *      pszSubName  - Subname to search for.
     *      pszValue    - Points to a buffer for the returned (sub)value.
     *      cbSubValue  - Size of the the return (sub)value buffer.
     *
     *  Return
     *      NOERROR     - The (sub)name was found.
     *      E_FAIL      - The (sub)name could not be found.
     *
     */
    STDMETHOD(FindValue)(THIS_ 
                         const TCHAR *pszName,
                         const TCHAR *pszSubName,
                         TCHAR *      pszValue,
                         DWORD        cbValue)          const   PURE;

    /* ************************************************************************
     *
     *  HRESULT IDAPacket::ReplaceNameValue
     *  (
     *      [IN]    const TCHAR *pszName,
     *      [IN]    const TCHAR *pszValue
     *  )
     *
     *  This method replaces an existing name and value in a DA packet.
     *
     *  Parameters
     *      pszName     - Name to append.
     *      pszValue    - Value to append.
     *
     *  Return
     *      NOERROR     - The name and value were updated.
     *      E_FAIL      - The name and value could not be updated.
     *
     *  Notes
     *      The pszValue string may itself be composed of many subnames and 
     *      subvalues.
     *
     */
    STDMETHOD(ReplaceNameValue)(THIS_
                                const TCHAR *pszName, 
                                const TCHAR *pszValue)          PURE;
};


/* ****************************************************************************
 *  
 *  Name
 *      IDAPassword
 *
 *  Purpose
 *      This interface might be implemented by clients who wish to access DA 
 *      services.  The interface is used to perform transformations of passwords 
 *      from text to 128 bit values.
 *
 *  Notes
 *      It is an optional interface.  Not implementing the interface causes 
 *      authentications to fail if a user chooses a service with any password 
 *      transformation parameters.
 *
 */
#undef  INTERFACE
#define INTERFACE   IDAPassword

DECLARE_INTERFACE_(IDAPassword, IUnknown)
{
    /* ************************************************************************
     *
     *  HRESULT IDAProcess::Transform
     *  (
     *      [IN]    const TCHAR *   pszPassword,
     *      [IN]    const TCHAR *   pszCharSet, 
     *      [IN]    const TCHAR *   pszConvCase,
     *      [IN]    const TCHAR *   pszHashFunc,
     *      [OUT]   BYTE *          pbPasswordXfrm
     *  )
     *
     *  This method transforms a password from text to a 128 bit value.
     *
     *  Parameters
     *      pszPassword     - Password as entered by the user.
     *      pszCharSet      - Service transformation character set.
     *      pszConvCase     - Service transformation case conversion.
     *      pszHashFunc     - Service transformation hash function.
     *      pbPasswordXfrm  - Pointer to 128 bits for the password 
     *                        transformation.
     *
     *  Return
     *      NOERROR - Password was successfully transformed to a 128 bit value.
     *      E_FAIL  - Unable to transform the password.
     *
     *  Notes
     *      If pszCharSet is "none", pszConvCase and pszHashFunc will be empty 
     *      strings.
     *
     */
    STDMETHOD(Transform)(const TCHAR *  pszPassword,
                         const TCHAR *  pszCharSet, 
                         const TCHAR *  pszConvCase,
                         const TCHAR *  pszHashFunc,
                         BYTE *         pbPasswdXfrm)   const   PURE;
};


/* ****************************************************************************
 *
 *  Name
 *      SDAAuthData
 *
 *  Purpose
 *      Used to package the needed parameters for IDAProcess::OnAuthorize.
 *
 */
struct  SDAAuthData
{
    const IDAPacket *   pIDAPacketIn;   // [IN]             Server response DA packet.
    const TCHAR *       pszHost;        // [IN]             Host for the outgoing DA packet.
    const TCHAR *       pszURI;         // [IN]             URI for the outgoing DA packet.
    const TCHAR *       pszMethod;      // [IN]             Method for the outgoing DA packet.
    IDAPacket *         pIDAPacketOut;  // [IN OUT]         New request DA packet.
    BOOL                bShowDialog;    // [IN]             Display authentication dialog?
    HWND                hParent;        // [IN OPTIONAL]    Parent for authentication dialog.
    TCHAR *             pszUsername;    // [IN OUT]         User name.
    WORD                wMaxUsername;   // [IN]             Maximum user name length.
    TCHAR *             pszRealmname;   // [IN OUT]         Realm name.
    TCHAR *             pszPassword;    // [IN OUT]         Password.
    WORD                wMaxPassword;   // [IN]             Maximum password length.
    const IDAPassword * pIDAPassword;   // [IN OPTIONAL]    Password transformation interface.
};


#define DA_MAXFIELD 64  // Maximum string parameter length.


/* ****************************************************************************
 *
 *  Name
 *      IDAProcess
 *
 *  Purpose
 *      This interface is implemented by the DA engine.  It is the interface 
 *      clients use to access DA services.
 *
 */
#undef  INTERFACE
#define INTERFACE   IDAProcess

DECLARE_INTERFACE_(IDAProcess, IUnknown)
{
    /* ************************************************************************
     *
     *  HRESULT IDAProcess::Cheat
     *  (
     *      [IN]        const TCHAR *   pszHost,
     *      [IN]        const TCHAR *   pszURI,
     *      [IN]        const TCHAR *   pszMethod,
     *      [IN OUT]    IDAPacket *     pIDAPacket
     *  )
     *
     *  This method performs DA cheating.
     *
     *  Parameters
     *      pszHost     - Name of the host to receive the outgoing request.
     *      pszURI      - URI of the outgoing request.
     *      pszMethod   - Method type of the outgoing request.
     *      pIDAPacket  - DA packet representing an outgoing request.
     *
     *  Return
     *      NOERROR         - An Authorization header was added to the 
     *                        outgoing request DA packet.
     *      S_FALSE         - Cheating failed, there is no DA cache 
     *                        information for the host.
     *      E_FAIL          - Outgoing request DA packet could not be 
     *                        updated.
     *      E_INVALIDARG    - A bad function parameter was passed, or the 
     *                        DA packet is not in the correct format, or IP
     *                        information could not be obtained for the 
     *                        hostname.
     *      E_UNEXPECTED    - Internal DA error.
     *
     */
    STDMETHOD(Cheat)(THIS_ 
                     const TCHAR *  pszHost, 
                     const TCHAR *  pszURI,
                     const TCHAR *  pszMethod,
                     IDAPacket *    pIDAPacket)                 const   PURE;
    
    /* ************************************************************************
     *
     *  HRESULT IDAProcess::On401Authenticate
     *  (
     *      [IN OUT]    SDAAuthData *pDAAuthData
     *  )
     *
     *  This method processes 401 WWW-Authenticate server response.
     *
     *  Parameters
     *      pDAAuthData - Authorization parameters.
     *
     *  Return
     *      NOERROR         - Successfully added an Authorization header to 
     *                        the outgoing request DA packet.
     *      E_FAIL          - The outgoing request DA packet could not be 
     *                        updated or IP information could not be obtained 
     *                        for the hostname.
     *      E_INVALIDARG    - A bad function parameter was passed, or the 
     *                        DA packet is not in the correct format, or IP
     *                        information could not be obtained for the 
     *                        hostname.
     *      E_UNEXPECTED    - Internal DA error.
     *
     */
    STDMETHOD(On401Authenticate)(THIS_
                                 SDAAuthData *pDAAuthData)      const   PURE;

    /* ************************************************************************
     *
     *  HRESULT IDAProcess::OnAuthenticate
     *  (
     *      [IN]    const TCHAR *       pszHost,
     *      [IN]    const IDAPacket *   pIDAPacket
     *  )
     *
     *  This method processes 200 WWW-Authenticate server response.
     *
     *  Parameters
     *      pszHost     - Name of the host to receive the outgoing request.
     *      pIDAPacket  - DA packet representing a server response.
     *
     *  Return
     *      NOERROR         - Successfully verified authentication.
     *      E_FAIL          - Authentication verification failed.
     *      E_INVALIDARG    - Bad function parameter was passed, or the DA
     *                        packet is not in the correct format, or IP
     *                        information could not be obtained for the 
     *                        hostname.
     *      E_UNEXPECTED    - Internal DA error.
     *
     */
    STDMETHOD(On200Authenticate)(THIS_
                                 const TCHAR *      pszHost, 
                                 const IDAPacket *  pIDAPacket) const   PURE;
};


#endif  // __DAINTF_H__


/* ****************************************************************************
 *
 *  GUIDs
 *
 */
// CLSID for the DA object {BCBD97D1-5F68-11CF-8370-000000000000}.
DEFINE_GUID(CLSID_DA, 
            0xBCBD97D1, 
            0x5F68, 
            0x11CF, 
            0x83, 
            0x70, 
            0x0, 
            0x0, 
            0x0, 
            0x0, 
            0x0, 
            0x0);

// IID for the IDAPacket interface {BCBD97D2-5F68-11CF-8370-000000000000}.
DEFINE_GUID(IID_IDAPacket, 
            0xBCBD97D2, 
            0x5F68, 
            0x11CF, 
            0x83, 
            0x70, 
            0x0, 
            0x0, 
            0x0, 
            0x0, 
            0x0, 
            0x0);

// IID for the IDAPassword interface {BCBD97D3-5F68-11CF-8370-000000000000}.
DEFINE_GUID(IID_IDAPassword,
            0xBCBD97D3, 
            0x5F68, 
            0x11CF, 
            0x83, 
            0x70, 
            0x0, 
            0x0, 
            0x0, 
            0x0, 
            0x0, 
            0x0);

// IID for the IDAProcess interface {BCBD97D4-5F68-11CF-8370-000000000000}.
DEFINE_GUID(IID_IDAProcess, 
            0xBCBD97D4, 
            0x5F68, 
            0x11CF, 
            0x83, 
            0x70, 
            0x0, 
            0x0, 
            0x0, 
            0x0, 
            0x0, 
            0x0);
